@echo off
:: Copyright (c) 2010 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This batch file will try to sync the root directory.

:: Shall skip automatic update?
IF "%DEPOT_TOOLS_UPDATE%" == "0" GOTO :EOF

:: We can't sync if ..\.svn\. doesn't exist.
IF NOT EXIST "%~dp0..\.svn\." GOTO :EOF

:: Sync the .. directory to update the bootstrap at the same time.
call svn -q up "%~dp0.."
