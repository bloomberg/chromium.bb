// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/link_infobar_delegate.h"

bool LinkInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  return true;
}

LinkInfoBarDelegate::LinkInfoBarDelegate(InfoBarTabService* infobar_service)
    : InfoBarDelegate(infobar_service) {
}

LinkInfoBarDelegate::~LinkInfoBarDelegate() {
}

LinkInfoBarDelegate* LinkInfoBarDelegate::AsLinkInfoBarDelegate() {
  return this;
}
