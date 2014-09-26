// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/components_resources.h"
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

  scoped_ptr<dom_distiller::proto::DomDistillerResult> distiller_result(
      new dom_distiller::proto::DomDistillerResult());
  bool found_content;
  if (value->IsType(base::Value::TYPE_NULL)) {
    found_content = false;
  } else {
    found_content =
        dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
            value, distiller_result.get());
    if (!found_content) {
      DVLOG(1) << "Unable to parse DomDistillerResult.";
    } else {
      if (distiller_result->has_timing_info()) {
        const dom_distiller::proto::TimingInfo& timing =
            distiller_result->timing_info();
        if (timing.has_markup_parsing_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.MarkupParsing",
              base::TimeDelta::FromMillisecondsD(timing.markup_parsing_time()));
        }
        if (timing.has_document_construction_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.DocumentConstruction",
              base::TimeDelta::FromMillisecondsD(
                  timing.document_construction_time()));
        }
        if (timing.has_article_processing_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.ArticleProcessing",
              base::TimeDelta::FromMillisecondsD(
                  timing.article_processing_time()));
        }
        if (timing.has_formatting_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.Formatting",
              base::TimeDelta::FromMillisecondsD(timing.formatting_time()));
        }
        if (timing.has_total_time()) {
          UMA_HISTOGRAM_TIMES(
              "DomDistiller.Time.DistillationTotal",
              base::TimeDelta::FromMillisecondsD(timing.total_time()));
        }
      }
      if (distiller_result->has_statistics_info()) {
        const dom_distiller::proto::StatisticsInfo& statistics =
            distiller_result->statistics_info();
        if (statistics.has_word_count()) {
          UMA_HISTOGRAM_CUSTOM_COUNTS(
            "DomDistiller.Statistics.WordCount",
            statistics.word_count(),
            1, 4000, 50);
        }
      }
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(distiller_page_callback_,
                 base::Passed(&distiller_result),
                 found_content));
}

}  // namespace dom_distiller
