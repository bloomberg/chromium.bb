// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/TestCommon.h"

namespace content {

namespace {

const char layoutTestsPattern[] = "/LayoutTests/";
const std::string::size_type layoutTestsPatternSize = sizeof(layoutTestsPattern) - 1;
const char fileUrlPattern[] = "file:/";
const char fileTestPrefix[] = "(file test):";
const char dataUrlPattern[] = "data:";
const std::string::size_type dataUrlPatternSize = sizeof(dataUrlPattern) - 1;

}  // namespace

std::string normalizeLayoutTestURL(const std::string& url)
{
    std::string result = url;
    size_t pos;
    if (!url.find(fileUrlPattern) && ((pos = url.find(layoutTestsPattern)) != std::string::npos)) {
        // adjust file URLs to match upstream results.
        result.replace(0, pos + layoutTestsPatternSize, fileTestPrefix);
    } else if (!url.find(dataUrlPattern)) {
        // URL-escape data URLs to match results upstream.
        std::string path = url.substr(dataUrlPatternSize);
        result.replace(dataUrlPatternSize, url.length(), path);
    }
    return result;
}

}  // namespace content
