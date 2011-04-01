:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

@echo off

:: gclient does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP gclient_runhooks@@@
call gclient runhooks --force

setlocal
call "%~dp0msvs_env.bat"
call "%~dp0cygwin_env.bat"
set CYGWIN=nodosfilewarning %CYGWIN%
bash buildbot/buildbot_windows-glibc-makefile.sh
if errorlevel 1 exit 1
endlocal

:: gsutil does not work when called from cygwin - python versions conflict.

echo @@@BUILD_STEP archive_build@@@
call ..\..\..\..\scripts\slave\gsutil -h Cache-Control:no-cache cp -a public-read tools\toolchain.tgz gs://nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/toolchain_win_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/@@@

echo @@@BUILD_STEP gyp_tests@@@
call vcvarsall.bat x86   && call devenv.com build\all.sln /build Release || (
  set RETCODE=0 || echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP scons_compile32@@@
call scons.bat -k -j 8 DOXYGEN=..\third_party\doxygen\win/doxygen --nacl_glibc --verbose --mode=opt-win,nacl,doc platform=x86-32 || (
  set RETCODE=0 || echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP scons_compile64@@@
call scons.bat -k -j 8 DOXYGEN=..\third_party\doxygen\win\doxygen --nacl_glibc --verbose --mode=opt-win,nacl,doc platform=x86-64 || (
  set RETCODE=0 || echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP small_tests64@@@
call scons.bat -k -j 8 --mode=dbg-host,nacl platform=x86-64 --nacl_glibc --verbose small_tests || (
  set RETCODE=0 || echo @@@STEP_FAILURE@@@)

# TODO(khim): add small_tests, medium_tests, large_tests, chrome_browser_tests.

exit %RETCODE%
