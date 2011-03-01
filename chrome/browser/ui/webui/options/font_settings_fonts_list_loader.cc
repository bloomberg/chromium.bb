// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_fonts_list_loader.h"

#include "chrome/browser/ui/webui/options/font_settings_utils.h"
#include "content/browser/browser_thread.h"

FontSettingsFontsListLoader::FontSettingsFontsListLoader(Observer* observer)
    : observer_(observer) {
}

FontSettingsFontsListLoader::~FontSettingsFontsListLoader() {
}

ListValue* FontSettingsFontsListLoader::GetFontsList() {
  return fonts_list_.get();
}

void FontSettingsFontsListLoader::StartLoadFontsList() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &FontSettingsFontsListLoader::GetFontsListOnFileThread));
}

void FontSettingsFontsListLoader::SetObserver(Observer* observer) {
  observer_ = observer;
}

void FontSettingsFontsListLoader::GetFontsListOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  fonts_list_.reset(FontSettingsUtilities::GetFontsList());

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FontSettingsFontsListLoader::FinishFontsListOnUIThread));
}

void FontSettingsFontsListLoader::FinishFontsListOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (observer_)
    observer_->FontsListHasLoaded();
}

