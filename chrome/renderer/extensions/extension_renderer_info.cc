// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_renderer_info.h"

#include "base/logging.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"

// static
std::vector<ExtensionRendererInfo>* ExtensionRendererInfo::extensions_ = NULL;

ExtensionRendererInfo::ExtensionRendererInfo() {
}

ExtensionRendererInfo::ExtensionRendererInfo(
    const ExtensionRendererInfo& that) {
  id_ = that.id_;
  web_extent_ = that.web_extent_;
  name_ = that.name_;
  icon_url_ = that.icon_url_;
}

ExtensionRendererInfo::~ExtensionRendererInfo() {
}

void ExtensionRendererInfo::Update(const ViewMsg_ExtensionRendererInfo& info) {
  id_ = info.id;
  web_extent_ = info.web_extent;
  name_ = info.name;
  location_ = info.location;
  icon_url_ = info.icon_url;
}

// static
void ExtensionRendererInfo::UpdateExtensions(
    const ViewMsg_ExtensionsUpdated_Params& params) {
  size_t count = params.extensions.size();
  if (!extensions_)
    extensions_ = new std::vector<ExtensionRendererInfo>(count);
  else
    extensions_->resize(count);

  for (size_t i = 0; i < count; ++i)
    extensions_->at(i).Update(params.extensions[i]);
}

// static
std::string ExtensionRendererInfo::GetIdByURL(const GURL& url) {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return url.host();

  ExtensionRendererInfo* info = GetByURL(url);
  if (!info)
    return "";

  return info->id();
}

// static
ExtensionRendererInfo* ExtensionRendererInfo::GetByURL(const GURL& url) {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return GetByID(url.host());

  if (!extensions_)
    return NULL;

  std::vector<ExtensionRendererInfo>::iterator i = extensions_->begin();
  for (; i != extensions_->end(); ++i) {
    if (i->web_extent_.ContainsURL(url))
      return &(*i);
  }

  return NULL;
}

// static
bool ExtensionRendererInfo::InSameExtent(const GURL& old_url,
                                         const GURL& new_url) {
  return GetByURL(old_url) == GetByURL(new_url);
}

// static
ExtensionRendererInfo* ExtensionRendererInfo::GetByID(
    const std::string& id) {

  if (!extensions_) {
    NOTREACHED();
    return NULL;
  }

  std::vector<ExtensionRendererInfo>::iterator i = extensions_->begin();
  for (; i != extensions_->end(); ++i) {
    if (i->id() == id)
      return &(*i);
  }
  return NULL;
}

// static
bool ExtensionRendererInfo::ExtensionBindingsAllowed(const GURL& url) {
  if (url.SchemeIs(chrome::kExtensionScheme))
    return true;

  if (!extensions_)
    return false;

  std::vector<ExtensionRendererInfo>::iterator i = extensions_->begin();
  for (; i != extensions_->end(); ++i) {
    if (i->location_ == Extension::COMPONENT &&
        i->web_extent_.ContainsURL(url))
      return true;
  }

  return false;
}
