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
call ..\..\..\..\scripts\slave\gsutil -h Cache-Control:no-cache cp -a public-read ^
 tools\toolchain.tgz ^
 gs://nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/toolchain_win_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r%BUILDBOT_GOT_REVISION%/@@@

:: Run tests

setlocal
call "%~dp0msvs_env.bat"

setlocal
echo @@@BUILD_STEP gyp_compile@@@
call vcvarsall.bat x86 && call devenv.com build\all.sln /build Release
:: if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP gyp_tests@@@
python_slave.exe trusted_test.py --config Release
:: if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal

setlocal
echo @@@BUILD_STEP scons_compile32@@@
call vcvarsall.bat x86 && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 --nacl_glibc -k --verbose --mode=opt-win,nacl,doc platform=x86-64
:: if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal

setlocal
echo @@@BUILD_STEP scons_compile64@@@
call vcvarsall.bat x64 && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 --nacl_glibc -k --verbose --mode=opt-win,nacl,doc platform=x86-32
:: if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal

setlocal
set RETCODE=0

echo @@@BUILD_STEP small_tests64@@@
call vcvarsall.bat x64 && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 --nacl_glibc -k --verbose --mode=dbg-win,nacl,doc small_tests platform=x86-64
:: if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

# TODO(khim): add medium_tests, large_tests, chrome_browser_tests.

exit /b %RETCODE%
