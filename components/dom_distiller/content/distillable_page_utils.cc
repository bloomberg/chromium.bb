// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/distillable_page_utils.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/page_features.h"
#include "content/public/browser/render_frame_host.h"
#include "grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {
void OnOGArticleJsResult(base::Callback<void(bool)> callback,
                         const base::Value* result) {
  bool success = false;
  if (result) {
    result->GetAsBoolean(&success);
  }
  callback.Run(success);
}

void OnExtractFeaturesJsResult(const DistillablePageDetector* detector,
                               base::Callback<void(bool)> callback,
                               const base::Value* result) {
  callback.Run(detector->Classify(CalculateDerivedFeaturesFromJSON(result)));
}
}  // namespace

void IsOpenGraphArticle(content::WebContents* web_contents,
                        base::Callback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, false));
    return;
  }
  std::string og_article_js = ResourceBundle::GetSharedInstance()
                                  .GetRawDataResource(IDR_IS_DISTILLABLE_JS)
                                  .as_string();
  main_frame->ExecuteJavaScript(base::UTF8ToUTF16(og_article_js),
                                base::Bind(OnOGArticleJsResult, callback));
}

void IsDistillablePage(content::WebContents* web_contents,
                       base::Callback<void(bool)> callback) {
  IsDistillablePageForDetector(web_contents,
                               DistillablePageDetector::GetDefault(), callback);
}

void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback) {
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, false));
    return;
  }
  std::string extract_features_js =
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(IDR_EXTRACT_PAGE_FEATURES_JS)
          .as_string();
  main_frame->ExecuteJavaScript(
      base::UTF8ToUTF16(extract_features_js),
      base::Bind(OnExtractFeaturesJsResult, detector, callback));
}

}  // namespace dom_distiller
