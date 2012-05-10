:: Copyright (c) 2012 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

echo on

set BITS=32
set VCBITS=x86

:: Picking out drive letter on which the build is happening so we can use it
:: for the temp directory.
set BUILD_DRIVE=%PATH:~0,1%

call buildbot\msvs_env.bat %BITS%

echo @@@BUILD_STEP clobber@@@
rd /s /q scons-out ^
 & rd /s /q build\Debug build\Release ^
 & rd /s /q build\Debug-Win32 build\Release-Win32 ^
 & rd /s /q build\Debug-x64 build\Release-x64

echo @@@BUILD_STEP cleanup_temp@@@
:: Selecting a temp directory on the same drive as the build.
:: Many of our bots have tightly packed C: drives, but plentiful E: drives.
set OLD_TEMP=%TEMP%
set TEMP=%BUILD_DRIVE%:\temp
set TMP=%TEMP%
mkdir %TEMP%
:: Safety check.
if "%OLD_TEMP%" equ "" goto SkipClean
:: Cleaning old temp directory to clear up all the nearly full bots out there.
del /S /Q "%OLD_TEMP%\*"
for /D %%I in ("%OLD_TEMP%\*") do rmdir /S /Q %%I
:: Cleaning new temp directory so we don't overflow in the future.
del /S /Q "%TEMP%\*"
for /D %%I in ("%TEMP%\*") do rmdir /S /Q %%I

echo on
echo @@@BUILD_STEP scons_compile@@@
call vcvarsall.bat %VCBITS%
call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=coverage-win,nacl platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP coverage@@@
call vcvarsall.bat %VCBITS%
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=coverage-win,nacl coverage platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP archive_coverage@@@
set GSUTIL="\b\build\scripts\slave\gsutil.bat"
set GSD_URL="http://gsdview.appspot.com/nativeclient-coverage2/revs"
set VARIANT_NAME="coverage-win-x86-%BITS%"
set COVERAGE_PATH="%VARIANT_NAME%/html/index.html"
if %BUILDBOT_REVISION% equ "" set BUILDBOT_REVISION=None
set LINK_URL="%GSD_URL%/%BUILDBOT_REVISION%/%COVERAGE_PATH%"
set GSD_BASE="gs://nativeclient-coverage2/revs"
set GS_PATH="%GSD_BASE%/%BUILDBOT_REVISION%/%VARIANT_NAME%"
python /b/build/scripts/slave/gsutil_cp_dir.py -a public-read ^
 scons-out/%VARIANT_NAME%/coverage %GS_PATH%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo @@@STEP_LINK@view@%LINK_URL%@@@
