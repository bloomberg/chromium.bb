// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DATABASE_MESSAGES_H_
#define CONTENT_COMMON_DATABASE_MESSAGES_H_

#include <stdint.h>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "url/origin.h"

#define IPC_MESSAGE_START DatabaseMsgStart

// Database messages sent from the renderer to the browser.

// Notifies the browser process that a new database has been opened
IPC_MESSAGE_CONTROL4(DatabaseHostMsg_Opened,
                     url::Origin /* origin */,
                     base::string16 /* database name */,
                     base::string16 /* database description */,
                     int64_t /* estimated size */)

// Notifies the browser process that a database might have been modified
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Modified,
                     url::Origin /* origin */,
                     base::string16 /* database name */)

// Notifies the browser process that a database is about to close
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Closed,
                     url::Origin /* origin */,
                     base::string16 /* database name */)

// Sent when a sqlite error indicates the database is corrupt.
IPC_MESSAGE_CONTROL3(DatabaseHostMsg_HandleSqliteError,
                     url::Origin /* origin */,
                     base::string16 /* database name */,
                     int /* error */)

#endif  // CONTENT_COMMON_DATABASE_MESSAGES_H_
