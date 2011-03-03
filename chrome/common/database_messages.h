// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DatabaseMsgStart

// Database messages sent from the browser to the renderer.

// Notifies the child process of the new database size
IPC_MESSAGE_CONTROL4(DatabaseMsg_UpdateSize,
                     string16 /* the origin */,
                     string16 /* the database name */,
                     int64 /* the new database size */,
                     int64 /* space available to origin */)

// Asks the child process to close a database immediately
IPC_MESSAGE_CONTROL2(DatabaseMsg_CloseImmediately,
                     string16 /* the origin */,
                     string16 /* the database name */)

// Database messages sent from the renderer to the browser.

// Sent by the renderer process to check whether access to web databases is
// granted by content settings. This may block and trigger a cookie prompt.
IPC_SYNC_MESSAGE_ROUTED4_1(DatabaseHostMsg_Allow,
                           std::string /* origin_url */,
                           string16 /* database name */,
                           string16 /* database display name */,
                           unsigned long /* estimated size */,
                           bool /* result */)

// Asks the browser process to open a DB file with the given name.
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_OpenFile,
                            string16 /* vfs file name */,
                            int /* desired flags */,
                            IPC::PlatformFileForTransit /* file_handle */)

// Asks the browser process to delete a DB file
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_DeleteFile,
                            string16 /* vfs file name */,
                            bool /* whether or not to sync the directory */,
                            int /* SQLite error code */)

// Asks the browser process to return the attributes of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileAttributes,
                            string16 /* vfs file name */,
                            int32 /* the attributes for the given DB file */)

// Asks the browser process to return the size of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileSize,
                            string16 /* vfs file name */,
                            int64 /* the size of the given DB file */)

// Notifies the browser process that a new database has been opened
IPC_MESSAGE_CONTROL4(DatabaseHostMsg_Opened,
                     string16 /* origin identifier */,
                     string16 /* database name */,
                     string16 /* database description */,
                     int64 /* estimated size */)

// Notifies the browser process that a database might have been modified
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Modified,
                     string16 /* origin identifier */,
                     string16 /* database name */)

// Notifies the browser process that a database is about to close
IPC_MESSAGE_CONTROL2(DatabaseHostMsg_Closed,
                     string16 /* origin identifier */,
                     string16 /* database name */)

