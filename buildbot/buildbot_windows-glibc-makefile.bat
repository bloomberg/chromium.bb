:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off

:: gclient does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP gclient_runhooks@@@
call gclient runhooks --force
rmdir /s /q %~dp0..\tools\toolchain

setlocal
echo @@@BUILD_STEP install mingw@@@
call python.bat buildbot\buildbot_mingw_install.py
if errorlevel 1 exit 1

echo @@@BUILD_STEP buildbot_windows-glibc-makefile@@@

call "%~dp0msvs_env.bat" 64
call "%~dp0cygwin_env.bat"
set CYGWIN=nodosfilewarning %CYGWIN%

"%~dp0..\cygwin\bin\bash" "buildbot/buildbot_windows-glibc-makefile.sh"
if errorlevel 1 exit 1
endlocal

:: Run tests

set INSIDE_TOOLCHAIN=1
call python.bat buildbot\buildbot_standard.py opt 64 glibc
