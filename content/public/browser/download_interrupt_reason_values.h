// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// File errors.

// Generic file operation failure.
// "File Error".
INTERRUPT_REASON(FILE_FAILED, 1)

// The file cannot be accessed due to security restrictions.
// The file cannot be accessed.
// "Access Denied".
INTERRUPT_REASON(FILE_ACCESS_DENIED, 2)

// There is not enough room on the drive.
// "Disk Full".
INTERRUPT_REASON(FILE_NO_SPACE, 3)

// The directory or file name is too long.
// "Path Too Long".
INTERRUPT_REASON(FILE_NAME_TOO_LONG, 5)

// The file is too large for the file system to handle.
// "File Too Large".
INTERRUPT_REASON(FILE_TOO_LARGE, 6)

// The file contains a virus.
// "Virus".
INTERRUPT_REASON(FILE_VIRUS_INFECTED, 7)

// The file was in use.
// Too many files are opened at once.
// We have run out of memory.
// "Temporary Problem".
INTERRUPT_REASON(FILE_TRANSIENT_ERROR, 10)


// Network errors.

// Generic network failure.
// "Network Error".
INTERRUPT_REASON(NETWORK_FAILED, 20)

// The network operation timed out.
// "Operation Timed Out".
INTERRUPT_REASON(NETWORK_TIMEOUT, 21)

// The network connection has been lost.
// "Connection Lost".
INTERRUPT_REASON(NETWORK_DISCONNECTED, 22)

// The server has gone down.
// "Server Down".
INTERRUPT_REASON(NETWORK_SERVER_DOWN, 23)


// Server responses.

// The server indicates that the operation has failed (generic).
// "Server Error".
INTERRUPT_REASON(SERVER_FAILED, 30)

// The server does not support range requests.
// Internal use only:  must restart from the beginning.
INTERRUPT_REASON(SERVER_NO_RANGE, 31)

// The download request does not meet the specified precondition.
// Internal use only:  the file has changed on the server.
INTERRUPT_REASON(SERVER_PRECONDITION, 32)

// The server does not have the requested data.
// "Unable to get file".
INTERRUPT_REASON(SERVER_BAD_CONTENT, 33)


// User input.

// The user canceled the download.
// "Canceled".
INTERRUPT_REASON(USER_CANCELED, 40)

// The user shut down the browser.
// Internal use only:  resume pending downloads if possible.
INTERRUPT_REASON(USER_SHUTDOWN, 41)


// Crash.

// The browser crashed.
// Internal use only:  resume pending downloads if possible.
INTERRUPT_REASON(CRASH, 50)
