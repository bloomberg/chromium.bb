// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SpellCheckMessageFilterTest, TestOverrideThread) {
  static const uint32 kSpellcheckMessages[] = {
    SpellCheckHostMsg_RequestDictionary::ID,
    SpellCheckHostMsg_NotifyChecked::ID,
    SpellCheckHostMsg_RespondDocumentMarkers::ID,
#if !defined(OS_MACOSX)
    SpellCheckHostMsg_CallSpellingService::ID,
#endif
  };
  scoped_refptr<SpellCheckMessageFilter> filter(new SpellCheckMessageFilter(0));
  content::BrowserThread::ID thread;
  IPC::Message message;
  for (size_t i = 0; i < arraysize(kSpellcheckMessages); ++i) {
    message.SetHeaderValues(
        0, kSpellcheckMessages[i], IPC::Message::PRIORITY_NORMAL);
    thread = content::BrowserThread::IO;
    filter->OverrideThreadForMessage(message, &thread);
    EXPECT_EQ(content::BrowserThread::UI, thread);
  }
}
