@echo off

:: Copyright 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can
:: be found in the LICENSE file.

:: gclient does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP gclient_runhooks@@@
call gclient runhooks --force

setlocal
call "%~dp0cygwin_env.bat"
set CYGWIN=nodosfilewarning %CYGWIN%
bash buildbot/buildbot_windows-glibc-makefile.sh
if errorlevel 1 exit 1
endlocal

:: gsutil does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP archive_build@@@
call ..\..\..\scripts\slave\gsutil -h Cache-Control:no-cache cp -a public-read tools\toolchain.tgz gs://nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/toolchain_win_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/@@@

