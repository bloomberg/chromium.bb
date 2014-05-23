// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/component_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

const char* kOptionsPlaceholder = "$$OPTIONS";

std::string GetDistillerScriptWithOptions(
    const dom_distiller::proto::DomDistillerOptions& options) {
  std::string script = ResourceBundle::GetSharedInstance()
                           .GetRawDataResource(IDR_DISTILLER_JS)
                           .as_string();
  if (script.empty()) {
    return "";
  }

  scoped_ptr<base::Value> options_value(
      dom_distiller::proto::json::DomDistillerOptions::WriteToValue(options));
  std::string options_json;
  if (!base::JSONWriter::Write(options_value.get(), &options_json)) {
    NOTREACHED();
  }
  size_t options_offset = script.find(kOptionsPlaceholder);
  DCHECK_NE(std::string::npos, options_offset);
  DCHECK_EQ(std::string::npos,
            script.find(kOptionsPlaceholder, options_offset + 1));
  script =
      script.replace(options_offset, strlen(kOptionsPlaceholder), options_json);
  return script;
}

}

DistilledPageInfo::DistilledPageInfo() {}

DistilledPageInfo::~DistilledPageInfo() {}

DistillerPageFactory::~DistillerPageFactory() {}

DistillerPage::DistillerPage() : ready_(true) {}

DistillerPage::~DistillerPage() {}

void DistillerPage::DistillPage(
    const GURL& gurl,
    const dom_distiller::proto::DomDistillerOptions options,
    const DistillerPageCallback& callback) {
  DCHECK(ready_);
  // It is only possible to distill one page at a time. |ready_| is reset when
  // the callback to OnDistillationDone happens.
  ready_ = false;
  distiller_page_callback_ = callback;
  DistillPageImpl(gurl, GetDistillerScriptWithOptions(options));
}

void DistillerPage::OnDistillationDone(const GURL& page_url,
                                       const base::Value* value) {
  DCHECK(!ready_);
  ready_ = true;

  scoped_ptr<DistilledPageInfo> page_info(new DistilledPageInfo());
  bool found_content = !value->IsType(base::Value::TYPE_NULL);
  if (found_content) {
    dom_distiller::proto::DomDistillerResult distiller_result =
        dom_distiller::proto::json::DomDistillerResult::ReadFromValue(value);

    page_info->title = distiller_result.title();
    page_info->html = distiller_result.distilled_content().html();
    page_info->next_page_url = distiller_result.pagination_info().next_page();
    page_info->prev_page_url = distiller_result.pagination_info().prev_page();
    for (int i = 0; i < distiller_result.image_urls_size(); ++i) {
      const std::string image_url = distiller_result.image_urls(i);
      if (GURL(image_url).is_valid()) {
        page_info->image_urls.push_back(image_url);
      }
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          distiller_page_callback_, base::Passed(&page_info), found_content));
}

}  // namespace dom_distiller
