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

DistilledPageInfo::MarkupArticle::MarkupArticle() {}

DistilledPageInfo::MarkupArticle::~MarkupArticle() {}

DistilledPageInfo::MarkupImage::MarkupImage() {}

DistilledPageInfo::MarkupImage::~MarkupImage() {}

DistilledPageInfo::MarkupInfo::MarkupInfo() {}

DistilledPageInfo::MarkupInfo::~MarkupInfo() {}

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
    const dom_distiller::proto::MarkupInfo& src_markup_info =
        distiller_result.markup_info();
    DistilledPageInfo::MarkupInfo& dst_markup_info = page_info->markup_info;
    dst_markup_info.title = src_markup_info.title();
    dst_markup_info.type = src_markup_info.type();
    dst_markup_info.url = src_markup_info.url();
    dst_markup_info.description = src_markup_info.description();
    dst_markup_info.publisher = src_markup_info.publisher();
    dst_markup_info.copyright = src_markup_info.copyright();
    dst_markup_info.author = src_markup_info.author();

    const dom_distiller::proto::MarkupArticle& src_article =
        src_markup_info.article();
    DistilledPageInfo::MarkupArticle& dst_article = dst_markup_info.article;
    dst_article.published_time = src_article.published_time();
    dst_article.modified_time = src_article.modified_time();
    dst_article.expiration_time = src_article.expiration_time();
    dst_article.section = src_article.section();
    for (int i = 0; i < src_article.authors_size(); ++i) {
      dst_article.authors.push_back(src_article.authors(i));
    }

    for (int i = 0; i < src_markup_info.images_size(); ++i) {
      const dom_distiller::proto::MarkupImage& src_image =
          src_markup_info.images(i);
      DistilledPageInfo::MarkupImage dst_image;
      dst_image.url = src_image.url();
      dst_image.secure_url = src_image.secure_url();
      dst_image.type = src_image.type();
      dst_image.caption = src_image.caption();
      dst_image.width = src_image.width();
      dst_image.height = src_image.height();
      dst_markup_info.images.push_back(dst_image);
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          distiller_page_callback_, base::Passed(&page_info), found_content));
}

}  // namespace dom_distiller
