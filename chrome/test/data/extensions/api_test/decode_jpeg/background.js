// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

bitmapArray = chrome.chromePrivate.decodeJPEG(jpegImageArray);
chrome.test.assertEq(bitmapArray.length, referenceBitmapArray.length);
chrome.test.assertEq(bitmapArray, referenceBitmapArray);
chrome.test.succeed();
