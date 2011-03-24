@echo off

:: Copyright 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can
:: be found in the LICENSE file.

:: TODO(pasko): convert this dummy script to do something useful once it is
:: invoked on the lucid64-glibc-makefile buildbot.

:: gclient does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

setlocal
call "%~dp0cygwin_env.bat"
echo PATH: %PATH%
bash buildbot_windows-glibc-makefile.sh
if errorlevel 1 goto :skip_publishing
endlocal

echo @@@BUILD_STEP archive_build@@@
..\..\..\scripts\slave\gsutil -h Cache-Control:no-cache cp -a public-read tools\toolchain.tgz gs://nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/toolchain_win_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/@@@

:skip_publishing
