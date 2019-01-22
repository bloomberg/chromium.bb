// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_url_loader_interceptor.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

void EmptyCallback(
    content::URLLoaderRequestInterceptor::RequestHandler callback) {}

TEST(PreviewsLitePageURLLoaderInterceptorTest, InterceptRequest) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  PreviewsLitePageURLLoaderInterceptor interceptor(1);

  base::HistogramTester histogram_tester;

  network::ResourceRequest request;
  request.url = GURL("https://google.com");
  request.resource_type = static_cast<int>(content::RESOURCE_TYPE_MAIN_FRAME);
  request.method = "GET";

  // Check that we don't trigger when previews are not allowed.
  request.previews_state = content::PREVIEWS_OFF;
  interceptor.MaybeCreateLoader(request, nullptr,
                                base::BindOnce(&EmptyCallback));

  histogram_tester.ExpectUniqueSample(
      "Previews.ServerLitePage.URLLoader.Attempted", false, 1);

  // Check that we trigger when previews are allowed.
  request.previews_state = content::LITE_PAGE_REDIRECT_ON;
  interceptor.MaybeCreateLoader(request, nullptr,
                                base::BindOnce(&EmptyCallback));

  histogram_tester.ExpectBucketCount(
      "Previews.ServerLitePage.URLLoader.Attempted", true, 1);
  histogram_tester.ExpectTotalCount(
      "Previews.ServerLitePage.URLLoader.Attempted", 2);
}

}  // namespace

}  // namespace previews
