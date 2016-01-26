// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_data_source_impl.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace content {

// static
WebUIDataSource* WebUIDataSource::Create(const std::string& source_name) {
  return new WebUIDataSourceImpl(source_name);
}

// static
void WebUIDataSource::Add(BrowserContext* browser_context,
                          WebUIDataSource* source) {
  URLDataManager::AddWebUIDataSource(browser_context, source);
}

// Internal class to hide the fact that WebUIDataSourceImpl implements
// URLDataSource.
class WebUIDataSourceImpl::InternalDataSource : public URLDataSource {
 public:
  InternalDataSource(WebUIDataSourceImpl* parent) : parent_(parent) {
  }

  ~InternalDataSource() override {}

  // URLDataSource implementation.
  std::string GetSource() const override { return parent_->GetSource(); }
  std::string GetMimeType(const std::string& path) const override {
    return parent_->GetMimeType(path);
  }
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const URLDataSource::GotDataCallback& callback) override {
    return parent_->StartDataRequest(path, render_process_id, render_frame_id,
                                     callback);
  }
  bool ShouldReplaceExistingSource() const override {
    return parent_->replace_existing_source_;
  }
  bool AllowCaching() const override { return false; }
  bool ShouldAddContentSecurityPolicy() const override {
    return parent_->add_csp_;
  }
  std::string GetContentSecurityPolicyObjectSrc() const override {
    if (parent_->object_src_set_)
      return parent_->object_src_;
    return URLDataSource::GetContentSecurityPolicyObjectSrc();
  }
  std::string GetContentSecurityPolicyFrameSrc() const override {
    if (parent_->frame_src_set_)
      return parent_->frame_src_;
    return URLDataSource::GetContentSecurityPolicyFrameSrc();
  }
  bool ShouldDenyXFrameOptions() const override {
    return parent_->deny_xframe_options_;
  }

 private:
  WebUIDataSourceImpl* parent_;
};

WebUIDataSourceImpl::WebUIDataSourceImpl(const std::string& source_name)
    : URLDataSourceImpl(
          source_name,
          new InternalDataSource(this)),
      source_name_(source_name),
      default_resource_(-1),
      add_csp_(true),
      object_src_set_(false),
      frame_src_set_(false),
      deny_xframe_options_(true),
      disable_set_font_strings_(false),
      replace_existing_source_(true) {
}

WebUIDataSourceImpl::~WebUIDataSourceImpl() {
}

void WebUIDataSourceImpl::AddString(const std::string& name,
                                    const base::string16& value) {
  localized_strings_.SetString(name, value);
}

void WebUIDataSourceImpl::AddString(const std::string& name,
                                    const std::string& value) {
  localized_strings_.SetString(name, value);
}

void WebUIDataSourceImpl::AddLocalizedString(const std::string& name,
                                             int ids) {
  localized_strings_.SetString(
      name, GetContentClient()->GetLocalizedString(ids));
}

void WebUIDataSourceImpl::AddLocalizedStrings(
    const base::DictionaryValue& localized_strings) {
  localized_strings_.MergeDictionary(&localized_strings);
}

void WebUIDataSourceImpl::AddBoolean(const std::string& name, bool value) {
  localized_strings_.SetBoolean(name, value);
}

void WebUIDataSourceImpl::SetJsonPath(const std::string& path) {
  json_path_ = path;
}

void WebUIDataSourceImpl::AddResourcePath(const std::string &path,
                                          int resource_id) {
  path_to_idr_map_[path] = resource_id;
}

void WebUIDataSourceImpl::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void WebUIDataSourceImpl::SetRequestFilter(
    const WebUIDataSource::HandleRequestCallback& callback) {
  filter_callback_ = callback;
}

void WebUIDataSourceImpl::DisableReplaceExistingSource() {
  replace_existing_source_ = false;
}

void WebUIDataSourceImpl::DisableContentSecurityPolicy() {
  add_csp_ = false;
}

void WebUIDataSourceImpl::OverrideContentSecurityPolicyObjectSrc(
    const std::string& data) {
  object_src_set_ = true;
  object_src_ = data;
}

void WebUIDataSourceImpl::OverrideContentSecurityPolicyFrameSrc(
    const std::string& data) {
  frame_src_set_ = true;
  frame_src_ = data;
}

void WebUIDataSourceImpl::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

std::string WebUIDataSourceImpl::GetSource() const {
  return source_name_;
}

std::string WebUIDataSourceImpl::GetMimeType(const std::string& path) const {
  // Remove the query string for to determine the mime type.
  std::string file_path = path.substr(0, path.find_first_of('?'));

  if (base::EndsWith(file_path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";

  if (base::EndsWith(file_path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  if (base::EndsWith(file_path, ".json", base::CompareCase::INSENSITIVE_ASCII))
    return "application/json";

  if (base::EndsWith(file_path, ".pdf", base::CompareCase::INSENSITIVE_ASCII))
    return "application/pdf";

  if (base::EndsWith(file_path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";

  return "text/html";
}

void WebUIDataSourceImpl::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const URLDataSource::GotDataCallback& callback) {
  if (!filter_callback_.is_null() &&
      filter_callback_.Run(path, callback)) {
    return;
  }

  if (!json_path_.empty() && path == json_path_) {
    SendLocalizedStringsAsJSON(callback);
    return;
  }

  int resource_id = default_resource_;
  std::map<std::string, int>::iterator result;
  // Remove the query string for named resource lookups.
  std::string file_path = path.substr(0, path.find_first_of('?'));
  result = path_to_idr_map_.find(file_path);
  if (result != path_to_idr_map_.end())
    resource_id = result->second;
  DCHECK_NE(resource_id, -1);
  SendFromResourceBundle(callback, resource_id);
}

void WebUIDataSourceImpl::SendLocalizedStringsAsJSON(
    const URLDataSource::GotDataCallback& callback) {
  std::string template_data;
  if (!disable_set_font_strings_) {
    std::string locale = GetContentClient()->browser()->GetApplicationLocale();
    webui::SetLoadTimeDataDefaults(locale, &localized_strings_);
  }

  webui::AppendJsonJS(&localized_strings_, &template_data);
  callback.Run(base::RefCountedString::TakeString(&template_data));
}

void WebUIDataSourceImpl::SendFromResourceBundle(
    const URLDataSource::GotDataCallback& callback, int idr) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      GetContentClient()->GetDataResourceBytes(idr));
  callback.Run(response.get());
}

}  // namespace content
