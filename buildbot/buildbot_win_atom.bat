:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

echo on

set MODE=%1
set BITS=%2

set RETCODE=0

if %MODE% equ "dbg" (set GYPMODE=Debug) else (set GYPMODE=Release)
if %BITS% equ 32 (set VCBITS=x86) else (set VCBITS=x64)

echo @@@BUILD_STEP extract_archive@@@
set GS_ARCHIVE=http://commondatastorage.googleapis.com/nativeclient-archive2
set GS_PLATFORM_ARCHIVE=%GS_ARCHIVE%/between_builders/vista64_%MODE%
set GS_URL=%GS_PLATFORM_ARCHIVE%/rev_%BUILDBOT_GOT_REVISION%/build.tgz
set OLD_PATH=%PATH%
set PATH=c:\cygwin\bin;c:\cygwin\usr\bin;%PATH%
curl -L %GS_URL% -o build.tgz && tar xvfz build.tgz --no-same-owner
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
set PATH=%OLD_PATH%

echo @@@BUILD_STEP small_tests@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc small_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP medium_tests@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc medium_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP large_tests@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc large_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP chrome_browser_tests@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

# TODO(mcgrathr): Drop support for non-IRT builds and remove this entirely.
# See http://code.google.com/p/nativeclient/issues/detail?id=1691
echo @@@BUILD_STEP chrome_browser_tests using IRT@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests irt=0
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP pyauto_tests@@@
call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 pyauto_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

exit /b %RETCODE%
