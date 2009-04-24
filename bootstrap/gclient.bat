@echo off
:: Copyright (c) 2009 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This batch file will try to sync the root directory and call back gclient.

setlocal

:: Shall skip automatic update?
IF "%DEPOT_TOOLS_UPDATE%" == "0" GOTO :gclient

:: We can't sync if ..\.svn\. doesn't exist.
IF NOT EXIST "%~dp0..\.svn\." GOTO :gclient

:: Sync the .. directory to update the bootstrap at the same time.
call svn -q up "%~dp0.."

:gclient
:: Defer control.
python "%~dp0\..\gclient.py" %*
