:: Copyright (c) 2012 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

echo on

set BITS=32
set VCBITS=x86

call buildbot\msvs_env.bat %BITS%
call vcvarsall.bat %VCBITS%

:: Standard script emits its own annotator tags.
python buildbot/buildbot_standard.py coverage %BITS% newlib --coverage
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP summarize coverage@@@
python tools/coverage_summary.py win-x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

:: Stop here and don't archive if on trybots.
if %BUILDBOT_SLAVE_TYPE% equ "" exit /b 0
if %BUILDBOT_SLAVE_TYPE% equ "Trybot" exit /b 0

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
