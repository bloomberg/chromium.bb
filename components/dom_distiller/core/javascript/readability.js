// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This include is processed by grit at build time.
<include src="../../../../third_party/readability/js/readability.js"/>

// Extracts long-form content from a page and returns and array where the first
// element is the article title, the second element is HTML containing the
// long-form content, and remaining elements are URLs for images referenced by
// that HTML. Each <img> tag in the HTML has an id field set to k - 2, which
// corresponds to a URL listed at index k in the array returned.
(function () {
    readability.init();
    var result = new Array(4);
    result[0] = readability.getArticleTitle();
    result[1] = readability.getDistilledArticleHTML();
    result[2] = readability.getNextPageLink();
    // TODO(shashishekhar): Add actual previous page link here.
    result[3] = '';
    return result.concat(readability.getImages());
}())
