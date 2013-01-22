// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

// Internal class to hide the fact that ChromeWebUIDataSource implements
// content::URLDataSource.
class ChromeWebUIDataSource::InternalDataSource
    : public content::URLDataSource {
 public:
  InternalDataSource(ChromeWebUIDataSource* parent) : parent_(parent) {
  }

  ~InternalDataSource() {
  }

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE {
    return parent_->GetSource();
  }
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE {
    return parent_->GetMimeType(path);
  }
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE {
    return parent_->StartDataRequest(path, is_incognito, callback);
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return parent_->add_csp_;
  }
  virtual std::string GetContentSecurityPolicyObjectSrc() const OVERRIDE {
    if (parent_->object_src_set_)
      return parent_->object_src_;
    return content::URLDataSource::GetContentSecurityPolicyObjectSrc();
  }
  virtual std::string GetContentSecurityPolicyFrameSrc() const OVERRIDE {
    if (parent_->frame_src_set_)
      return parent_->frame_src_;
    return content::URLDataSource::GetContentSecurityPolicyFrameSrc();
  }
  virtual bool ShouldDenyXFrameOptions() const OVERRIDE {
    return parent_->deny_xframe_options_;
  }

 private:
  ChromeWebUIDataSource* parent_;
};

content::WebUIDataSource* ChromeWebUIDataSource::Create(
    const std::string& source_name) {
  return new ChromeWebUIDataSource(source_name);
}

ChromeWebUIDataSource::ChromeWebUIDataSource(const std::string& source_name)
    : URLDataSourceImpl(
          source_name,
          new InternalDataSource(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      source_name_(source_name),
      default_resource_(-1),
      json_js_format_v2_(false),
      add_csp_(true),
      object_src_set_(false),
      frame_src_set_(false),
      deny_xframe_options_(true) {
}

ChromeWebUIDataSource::~ChromeWebUIDataSource() {
}

void ChromeWebUIDataSource::AddString(const std::string& name,
                                      const string16& value) {
  localized_strings_.SetString(name, value);
}

void ChromeWebUIDataSource::AddString(const std::string& name,
                                      const std::string& value) {
  localized_strings_.SetString(name, value);
}

void ChromeWebUIDataSource::AddLocalizedString(const std::string& name,
                                               int ids) {
  localized_strings_.SetString(name, l10n_util::GetStringUTF16(ids));
}

void ChromeWebUIDataSource::AddLocalizedStrings(
    const DictionaryValue& localized_strings) {
  localized_strings_.MergeDictionary(&localized_strings);
}

void ChromeWebUIDataSource::AddBoolean(const std::string& name, bool value) {
  localized_strings_.SetBoolean(name, value);
}

void ChromeWebUIDataSource::SetJsonPath(const std::string& path) {
  json_path_ = path;
}

void ChromeWebUIDataSource::SetUseJsonJSFormatV2() {
  json_js_format_v2_ = true;
}

void ChromeWebUIDataSource::AddResourcePath(const std::string &path,
                                            int resource_id) {
  path_to_idr_map_[path] = resource_id;
}

void ChromeWebUIDataSource::SetDefaultResource(int resource_id) {
  default_resource_ = resource_id;
}

void ChromeWebUIDataSource::SetRequestFilter(
    const content::WebUIDataSource::HandleRequestCallback& callback) {
  filter_callback_ = callback;
}

void ChromeWebUIDataSource::DisableContentSecurityPolicy() {
  add_csp_ = false;
}

void ChromeWebUIDataSource::OverrideContentSecurityPolicyObjectSrc(
    const std::string& data) {
  object_src_set_ = true;
  object_src_ = data;
}

void ChromeWebUIDataSource::OverrideContentSecurityPolicyFrameSrc(
    const std::string& data) {
  frame_src_set_ = true;
  frame_src_ = data;
}

void ChromeWebUIDataSource::DisableDenyXFrameOptions() {
  deny_xframe_options_ = false;
}

std::string ChromeWebUIDataSource::GetSource() {
  return source_name_;
}

std::string ChromeWebUIDataSource::GetMimeType(const std::string& path) const {
  if (EndsWith(path, ".js", false))
    return "application/javascript";

  if (EndsWith(path, ".json", false))
    return "application/json";

  if (EndsWith(path, ".pdf", false))
    return "application/pdf";

  return "text/html";
}

void ChromeWebUIDataSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
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
  result = path_to_idr_map_.find(path);
  if (result != path_to_idr_map_.end())
    resource_id = result->second;
  DCHECK_NE(resource_id, -1);
  SendFromResourceBundle(callback, resource_id);
}

void ChromeWebUIDataSource::SendLocalizedStringsAsJSON(
    const content::URLDataSource::GotDataCallback& callback) {
  std::string template_data;
  web_ui_util::SetFontAndTextDirection(&localized_strings_);

  scoped_ptr<webui::UseVersion2> version2;
  if (json_js_format_v2_)
    version2.reset(new webui::UseVersion2);

  webui::AppendJsonJS(&localized_strings_, &template_data);
  callback.Run(base::RefCountedString::TakeString(&template_data));
}

void ChromeWebUIDataSource::SendFromResourceBundle(
    const content::URLDataSource::GotDataCallback& callback, int idr) {
  scoped_refptr<base::RefCountedStaticMemory> response(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          idr));
  callback.Run(response);
}
