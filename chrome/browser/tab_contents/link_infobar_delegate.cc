// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/link_infobar_delegate.h"

bool LinkInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

LinkInfoBarDelegate::LinkInfoBarDelegate(InfoBarTabHelper* infobar_helper)
    : InfoBarDelegate(infobar_helper) {
}

LinkInfoBarDelegate::~LinkInfoBarDelegate() {
}

LinkInfoBarDelegate* LinkInfoBarDelegate::AsLinkInfoBarDelegate() {
  return this;
}
