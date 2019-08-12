@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.
setlocal

:: Shall skip automatic update?
IF "%DEPOT_TOOLS_UPDATE%" == "0" GOTO :SKIP_UPDATE

:: Synchronize the root directory before deferring control back to gclient.py.
call "%~dp0update_depot_tools.bat" %*

:SKIP_UPDATE

:: Ensure that "depot_tools" is somewhere in PATH so this tool can be used
:: standalone, but allow other PATH manipulations to take priority.
set PATH=%PATH%;%~dp0

:: Defer control.
%~dp0\vpython "%~dp0\gclient.py" %*
