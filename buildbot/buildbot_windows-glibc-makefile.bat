:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off

:: gclient does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP gclient_runhooks@@@
call gclient runhooks --force
rmdir /s /q %~dp0..\tools\toolchain

setlocal
call "%~dp0msvs_env.bat" 64
call "%~dp0cygwin_env.bat"
set CYGWIN=nodosfilewarning %CYGWIN%
bash buildbot/buildbot_windows-glibc-makefile.sh
if errorlevel 1 exit 1
endlocal

if "%BUILDBOT_SLAVE_TYPE%"=="" goto SkipUpload
if "%BUILDBOT_SLAVE_TYPE%"=="Trybot" goto SkipUpload

:: gsutil does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP archive_build@@@
for %%s in (gz gz.sha1hash bz2 bz2.sha1hash xz xz.sha1hash) do call^
  ..\..\..\..\scripts\slave\gsutil cp -a public-read ^
   tools\toolchain.tar.%%s ^
   gs://nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/toolchain_win_x86.tar.%%s
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/@@@
:SkipUpload

:: Run tests

set INSIDE_TOOLCHAIN=1
python buildbot\buildbot_standard.py opt 64 glibc

