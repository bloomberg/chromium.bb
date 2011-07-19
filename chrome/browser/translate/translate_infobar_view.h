// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_
#pragma once

// This interface should be implemented by classes that are the view (in the MVC
// sense) of the TranslateInfoBarDelegate (which is the model).
class TranslateInfoBarView {
 public:
  virtual void OriginalLanguageChanged() {}
  virtual void TargetLanguageChanged() {}

 protected:
  virtual ~TranslateInfoBarView() {}
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_VIEW_H_
