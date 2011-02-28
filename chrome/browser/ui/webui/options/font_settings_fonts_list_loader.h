// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_FONTS_LIST_LOADER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_FONTS_LIST_LOADER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/values.h"

// This class allows asynchronous retrieval of the system fonts list. The
// loading of the fonts is handled by a platform specific implementation, but
// the retrieval is done on the File thread so that UI does not block for what
// could be a long operation (if the user has a large number of fonts.)
class FontSettingsFontsListLoader
    : public base::RefCountedThreadSafe<FontSettingsFontsListLoader> {
 public:
  // Any clients of this class must implement this observer interface in
  // order to be called back when the fonts list has been loaded.
  class Observer {
   public:
    virtual void FontsListHasLoaded() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Pass in an observer, often 'this'.
  explicit FontSettingsFontsListLoader(Observer* observer);

  // Get the font list. This must only be called after receiveing the
  // FontsListHasLoaded() notification.
  ListValue* GetFontsList();

  // Start loading of the fonts list. The observer will be notified when this
  // operation has completed.
  void StartLoadFontsList();

  // Set the observer. This class does not take ownership of the observer.
  // The observer can be NULL.
  void SetObserver(Observer* observer);

 private:
  friend class base::RefCountedThreadSafe<FontSettingsFontsListLoader>;

  ~FontSettingsFontsListLoader();

  void GetFontsListOnFileThread();
  void FinishFontsListOnUIThread();

  scoped_ptr<ListValue> fonts_list_;
  Observer* observer_;  // weak

  DISALLOW_COPY_AND_ASSIGN(FontSettingsFontsListLoader);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_FONT_SETTINGS_FONTS_LIST_LOADER_H_

