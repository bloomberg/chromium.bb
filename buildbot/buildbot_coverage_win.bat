rem Copyright (c) 2011 The Native Client Authors. All rights reserved.
rem Use of this source code is governed by a BSD-style license that can be
rem found in the LICENSE file.

echo on

call buildbot\msvs_env.bat

set BITS=32
set VCBITS=x86

echo @@@BUILD_STEP gclient_runhooks@@@
cmd /c gclient runhooks
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP clobber@@@
rd /s /q scons-out ^
 & rd /s /q toolchain ^
 & rd /s /q build\Debug build\Release ^
 & rd /s /q build\Debug-Win32 build\Release-Win32 ^
 & rd /s /q build\Debug-x64 build\Release-x64

echo @@@BUILD_STEP partial_sdk@@@
call scons.bat --verbose --mode=nacl_extra_sdk platform=x86-%BITS% ^
 --download extra_sdk_update_header install_libpthread extra_sdk_update
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo on
echo @@@BUILD_STEP scons_compile@@@
call vcvarsall.bat %VCBITS%
call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=coverage-win,nacl,doc platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP coverage@@@
call vcvarsall.bat %VCBITS%
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=coverage-win,nacl,doc coverage platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP archive_coverage@@@
set GSUTIL="\b\build\scripts\slave\gsutil.bat -h Cache-Control:no-cache"
set GSD_URL="http://gsdview.appspot.com/nativeclient-coverage2/revs"
set VARIANT_NAME="coverage-win-x86-%BITS%"
set COVERAGE_PATH="%VARIANT_NAME%/html/index.html"
if %BUILDBOT_REVISION% equ "" set BUILDBOT_REVISION=None
set LINK_URL="%GSD_URL%/%BUILDBOT_REVISION%/%COVERAGE_PATH%"
set GSD_BASE="gs://nativeclient-coverage2/revs"
set GS_PATH="%GSD_BASE%/%BUILDBOT_REVISION%/%VARIANT_NAME%"
python /b/build/scripts/slave/gsutil_cp_dir.py ^
 scons-out/%VARIANT_NAME%/coverage %GS_PATH%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo @@@STEP_LINK@view@%LINK_URL%@@@
