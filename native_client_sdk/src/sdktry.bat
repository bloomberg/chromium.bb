@echo off

:: Copyright (c) 2012 The The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

:: TODO(noelallen) Share list with POSIX
gcl try %* -b naclsdkm-mac -b naclsdkm-linux -b naclsdkm-linux ^
-b naclsdkm-pnacl-linux -b naclsdkm-pnacl-mac -b naclsdkm-windows32 ^
-b naclsdkm-windows64 -S svn://svn.chromium.org/chrome-try/try-nacl