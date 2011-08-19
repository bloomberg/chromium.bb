// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace {

// A corrupted BDICT data used in DeleteCorruptedBDICT. Please do not use this
// BDICT data for other tests.
const uint8 kCorruptedBDICT[] = {
  0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00,
  0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
  0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35,
  0x32, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
  0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
  0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xe6, 0x49, 0x00, 0x68, 0x02,
  0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
};

}  // namespace

class SpellCheckHostBrowserTest : public InProcessBrowserTest {
};

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellCheckHostBrowserTest, DeleteCorruptedBDICT) {
  // Write the corrupted BDICT data to create a corrupted BDICT file.
  FilePath dict_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  FilePath bdict_path =
      SpellCheckCommon::GetVersionedFileName("en-US", dict_dir);

  size_t actual = file_util::WriteFile(bdict_path,
      reinterpret_cast<const char*>(kCorruptedBDICT),
      arraysize(kCorruptedBDICT));
  EXPECT_EQ(arraysize(kCorruptedBDICT), actual);

  // Attach an event to the SpellCheckHost object so we can receive its status
  // updates.
  base::WaitableEvent event(true, false);
  SpellCheckHost::AttachStatusEvent(&event);

  // Initialize the SpellCheckHost object with the corrupted BDICT file created
  // above. The SpellCheckHost object will send a BDICT_CORRUPTED event.
  browser()->profile()->ReinitializeSpellCheckHost(false);

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  int type = SpellCheckHost::WaitStatusEvent();
  EXPECT_EQ(SpellCheckHost::BDICT_CORRUPTED, type);
  if (file_util::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(file_util::Delete(bdict_path, true));
  }
}
