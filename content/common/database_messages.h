// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include <stdint.h>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "url/origin.h"

#define IPC_MESSAGE_START DatabaseMsgStart

// Database messages sent from the browser to the renderer.

// Notifies the child process of the new database size
IPC_MESSAGE_CONTROL3(DatabaseMsg_UpdateSize,
                     url::Origin /* the origin */,
                     base::string16 /* the database name */,
                     int64_t /* the new database size */)

// Asks the child process to close a database immediately
IPC_MESSAGE_CONTROL2(DatabaseMsg_CloseImmediately,
                     url::Origin /* the origin */,
                     base::string16 /* the database name */)

// Database messages sent from the renderer to the browser.

// Asks the browser process to open a DB file with the given name.
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_OpenFile,
                            base::string16 /* vfs file name */,
                            int /* desired flags */,
                            IPC::PlatformFileForTransit /* file_handle */)

// Asks the browser process to delete a DB file
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_DeleteFile,
                            base::string16 /* vfs file name */,
                            bool /* whether or not to sync the directory */,
                            int /* SQLite error code */)

// Asks the browser process to return the attributes of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileAttributes,
                            base::string16 /* vfs file name */,
                            int32_t /* the attributes for the given DB file */)

// Asks the browser process to return the size of a DB file
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetFileSize,
                            base::string16 /* vfs file name */,
                            int64_t /* the size of the given DB file */)

// Asks the browser process for the amount of space available to an origin
IPC_SYNC_MESSAGE_CONTROL1_1(DatabaseHostMsg_GetSpaceAvailable,
                            url::Origin /* origin */,
                            int64_t /* remaining space available */)

// Asks the browser set the size of a DB file
IPC_SYNC_MESSAGE_CONTROL2_1(DatabaseHostMsg_SetFileSize,
                            base::string16 /* vfs file name */,
                            int64_t /* expected size of the given DB file */,
                            bool /* indicates success */)

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
