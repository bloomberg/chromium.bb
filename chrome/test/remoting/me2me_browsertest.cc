// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/waiter.h"

namespace remoting {

class Me2MeBrowserTest : public RemoteDesktopBrowserTest {
 protected:
  void TestKeyboardInput();
  void TestMouseInput();
};

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Local_Host) {
  VerifyInternetAccess();

  Install();

  LaunchChromotingApp();

  // Authorize, Authenticate, and Approve.
  Auth();

  StartMe2Me();

  ConnectToLocalHost();

  TestKeyboardInput();

  TestMouseInput();

  DisconnectMe2Me();

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Remote_Host) {
  VerifyInternetAccess();

  Install();

  LaunchChromotingApp();

  // Authorize, Authenticate, and Approve.
  Auth();

  StartMe2Me();

  ConnectToRemoteHost(remote_host_name());

  // TODO(weitaosu): Find a way to verify keyboard input injection.
  // We cannot use TestKeyboardInput because it assumes
  // that the client and the host are on the same machine.

  DisconnectMe2Me();

  Cleanup();
}

// Typing a command which writes to a temp file and then verify the contents of
// the file.
void Me2MeBrowserTest::TestKeyboardInput() {
  // Start a terminal windows with ctrl+alt+T
  SimulateKeyPressWithCode(ui::VKEY_T, "KeyT", true, false, true, false);

  // Wait for the keyboard events to be sent to and processed by the host.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromMilliseconds(300)).Wait());

  base::FilePath temp_file;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_file));

  // Write some text into the temp file.
  std::string text = "Abigail";
  std::string command = "echo -n " + text + " > " +
                        temp_file.MaybeAsASCII() + "\n";
  SimulateStringInput(command);
  SimulateStringInput("exit\n");

  // Wait for the keyboard events to be sent to and processed by the host.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(1)).Wait());

  // Read the content of the temp file.
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(temp_file, &content));

  LOG(INFO) << temp_file.value();

  EXPECT_EQ(text, content);

  EXPECT_TRUE(base::DeleteFile(temp_file, false));
}

void Me2MeBrowserTest::TestMouseInput() {
  SimulateMouseLeftClickAt(10, 50);
  // TODO: Verify programatically the mouse events are received by the host.
  // This will be tricky as it depends on the host OS, window manager, desktop
  // layout, and screen resolution. Until then we need to visually verify that
  // "Dash Home" is clicked on a Unity window manager.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(5)).Wait());
}

}  // namespace remoting
