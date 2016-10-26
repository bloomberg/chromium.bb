// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_new_window_client.h"

namespace ash {

TestNewWindowClient::TestNewWindowClient() {}

TestNewWindowClient::~TestNewWindowClient() {}

void TestNewWindowClient::NewTab() {}

void TestNewWindowClient::NewWindow(bool incognito) {}

void TestNewWindowClient::OpenFileManager() {}

void TestNewWindowClient::OpenCrosh() {}

void TestNewWindowClient::OpenGetHelp() {}

void TestNewWindowClient::RestoreTab() {}

void TestNewWindowClient::ShowKeyboardOverlay() {}

void TestNewWindowClient::ShowTaskManager() {}

void TestNewWindowClient::OpenFeedbackPage() {}

}  // namespace ash
