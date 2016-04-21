// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/web_ui_ios_data_source_impl.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "ios/web/public/web_client.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace web {

// static
WebUIIOSDataSource* WebUIIOSDataSource::Create(const std::string& source_name) {
  return new WebUIIOSDataSourceImpl(source_name);
}

// static
void WebUIIOSDataSource::Add(BrowserState* browser_state,
                             WebUIIOSDataSource* source) {
  URLDataManagerIOS::AddWebUIIOSDataSource(browser_state, source);
}

// Internal class to hide the fact that WebUIIOSDataSourceImpl implements
// URLDataSourceIOS.
class WebUIIOSDataSourceImpl::InternalDataSource : public URLDataSourceIOS {
 public:
  InternalDataSource(WebUIIOSDataSourceImpl* parent) : parent_(parent) {}

  ~InternalDataSource() override {}

  // URLDataSourceIOS implementation.
  std::string GetSource() const override { return parent_->GetSource(); }
  std::string GetMimeType(const std::string& path) const override {
    return parent_->GetMimeType(path);
  }
  void StartDataRequest(
      const std::string& path,
      const URLDataSourceIOS::GotDataCallback& callback) override {
    return parent_->StartDataRequest(path, callback);
  }
  bool ShouldReplaceExistingSource() const override {
    return parent_->replace_existing_source_;
  }
  bool AllowCaching() const override { return false; }
  bool ShouldDenyXFrameOptions() const override {
    return parent_->deny_xframe_options_;
  }

 private:
  WebUIIOSDataSourceImpl* parent_;
};

WebUIIOSDataSourceImpl::WebUIIOSDataSourceImpl(const std::string& source_name)
    : URLDataSourceIOSImpl(source_name, new InternalDataSource(this)),
      source_name_(source_name),
      default_resource_(-1),
      deny_xframe_options_(true),
      replace_existing_source_(true) {}

WebUIIOSDataSourceImpl::~WebUIIOSDataSourceImpl() {}

void WebUIIOSDataSourceImpl::AddString(const std::string& name,
                                       const base::string16& value) {
  localized_strings_.SetString(name, value);
}

void WebUIIOSDataSourceImpl::AddString(const std::string& name,
                                       const std::string& value) {
  localized_strings_.SetString(name, value);
}

void WebUIIOSDataSourceImpl::AddLocalizedString(const std::string& name,
                                                int ids) {
  localized_strings_.SetString(name, GetWebClient()->GetLocalizedString(ids));
}

void WebUIIOSDataSourceImpl::AddBoolean(const std::string& name, bool value) {
  localized_strings_.SetBoolean(name, value);
}

void WebUIIOSDataSourceImpl::SetJsonPath(const std::string& path) {
  json_path_ = path;
}

void WebUIIOSDataSourceImpl::AddResourcePath(const std::string& path,
                                             int resource_id) {
  path_to_idr_map_[path] = resource_id;
}

void WebUIIOSDataSourceImpl::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void WebUIIOSDataSourceImpl::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

std::string WebUIIOSDataSourceImpl::GetSource() const {
  return source_name_;
}

std::string WebUIIOSDataSourceImpl::GetMimeType(const std::string& path) const {
  if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
    return "application/javascript";

  if (base::EndsWith(path, ".json", base::CompareCase::INSENSITIVE_ASCII))
    return "application/json";

  if (base::EndsWith(path, ".pdf", base::CompareCase::INSENSITIVE_ASCII))
    return "application/pdf";

  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII))
    return "text/css";

  if (base::EndsWith(path, ".svg", base::CompareCase::INSENSITIVE_ASCII))
    return "image/svg+xml";

  return "text/html";
}

void WebUIIOSDataSourceImpl::StartDataRequest(
    const std::string& path,
    const URLDataSourceIOS::GotDataCallback& callback) {
  if (!json_path_.empty() && path == json_path_) {
    SendLocalizedStringsAsJSON(callback);
    return;
  }

  int resource_id = default_resource_;
  std::map<std::string, int>::iterator result;
  result = path_to_idr_map_.find(path);
  if (result != path_to_idr_map_.end())
    resource_id = result->second;
  DCHECK_NE(resource_id, -1);
  SendFromResourceBundle(callback, resource_id);
}

void WebUIIOSDataSourceImpl::SendLocalizedStringsAsJSON(
    const URLDataSourceIOS::GotDataCallback& callback) {
  std::string template_data;
  webui::SetLoadTimeDataDefaults(web::GetWebClient()->GetApplicationLocale(),
                                 &localized_strings_);

  webui::AppendJsonJS(&localized_strings_, &template_data);
  callback.Run(base::RefCountedString::TakeString(&template_data));
}

void WebUIIOSDataSourceImpl::SendFromResourceBundle(
    const URLDataSourceIOS::GotDataCallback& callback,
    int idr) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      GetWebClient()->GetDataResourceBytes(idr));
  callback.Run(response);
}

}  // namespace web
