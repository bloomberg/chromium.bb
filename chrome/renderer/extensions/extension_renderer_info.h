// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_RENDERER_INFO_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_RENDERER_INFO_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_extent.h"
#include "googleurl/src/gurl.h"

struct ViewMsg_ExtensionsUpdated_Params;
struct ViewMsg_ExtensionRendererInfo;

// Extension information needed in the renderer process along with static
// methods to look up information about the currently loaded extensions.
class ExtensionRendererInfo {
 public:
  ExtensionRendererInfo();
  ExtensionRendererInfo(const ExtensionRendererInfo& that);
  ~ExtensionRendererInfo();

  const std::string& id() const { return id_; }
  const ExtensionExtent& web_extent() const { return web_extent_; }
  const std::string& name() const { return name_; }
  const GURL& icon_url() const { return icon_url_; }

  // Replace the list of extensions with those provided in |params|.
  static void UpdateExtensions(const ViewMsg_ExtensionsUpdated_Params& params);

  // Returns the extension ID that the given URL is a part of, or empty if
  // none. This includes web URLs that are part of an extension's web extent.
  static std::string GetIdByURL(const GURL& url);

  // Returns the ExtensionRendererInfo that the given URL is a part of, or NULL
  // if none. This includes web URLs that are part of an extension's web extent.
  // NOTE: This can return NULL if called before UpdateExtensions receives
  // bulk extension data (e.g. if called from
  // EventBindings::HandleContextCreated)
  static ExtensionRendererInfo* GetByURL(const GURL& url);

  // Returns true if |new_url| is in the extent of the same extension as
  // |old_url|.  Also returns true if neither URL is in an app.
  static bool InSameExtent(const GURL& old_url, const GURL& new_url);

  // Look up an ExtensionInfo object by id.
  static ExtensionRendererInfo* GetByID(const std::string& id);

  // Returns true if |url| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  static bool ExtensionBindingsAllowed(const GURL& url);

 private:
  void Update(const ViewMsg_ExtensionRendererInfo& info);

  FRIEND_TEST_ALL_PREFIXES(ExtensionRendererInfoTest, ExtensionRendererInfo);

  std::string id_;
  ExtensionExtent web_extent_;
  std::string name_;
  Extension::Location location_;
  GURL icon_url_;

  // static
  static std::vector<ExtensionRendererInfo>* extensions_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_RENDERER_INFO_H_
