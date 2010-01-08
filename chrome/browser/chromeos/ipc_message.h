// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_IPC_MESSAGE_H_
#define CHROME_BROWSER_CHROMEOS_IPC_MESSAGE_H_

enum IpcMessage {
  EMIT_LOGIN = 'a',
  START_SESSION = 'b',
  STOP_SESSION = 'c',
  FAILED = 'f',
};

#endif  // CHROME_BROWSER_CHROMEOS_IPC_MESSAGE_H_
