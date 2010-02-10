// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <string>

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/common/notification_type.h"

namespace printing {

PrintViewManager::PrintViewManager(TabContents& owner) : owner_(owner) {
  NOTIMPLEMENTED();
}

PrintViewManager::~PrintViewManager() {
  NOTIMPLEMENTED();
}

void PrintViewManager::Stop() {
  NOTIMPLEMENTED();
}

bool PrintViewManager::OnRenderViewGone(RenderViewHost* render_view_host) {
  NOTIMPLEMENTED();
  return true;
}

std::wstring PrintViewManager::RenderSourceName() {
  NOTIMPLEMENTED();
  return std::wstring();
}

GURL PrintViewManager::RenderSourceUrl() {
  NOTIMPLEMENTED();
  return GURL();
}

void PrintViewManager::DidGetPrintedPagesCount(int cookie, int number_pages) {
  NOTIMPLEMENTED();
}

void PrintViewManager::DidPrintPage(
    const ViewHostMsg_DidPrintPage_Params& params) {
  NOTIMPLEMENTED();
}

void PrintViewManager::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  NOTIMPLEMENTED();
}

}  // namespace printing
