:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

echo on

call buildbot\msvs_env.bat

set MODE=%1
set BITS=%2

set RETCODE=0

if %MODE% equ "dbg" (set GYPMODE=Debug) else (set GYPMODE=Release)
if %BITS% equ 32 (set VCBITS=x86) else (set VCBITS=x64)

:: Skip over hooks, clobber, and partial_sdk when run inside the toolchain
:: build as the toolchain takes care or the clobber, hooks aren't needed, and
:: partial_sdk really shouldn't be needed.
if "%INSIDE_TOOLCHAIN%" neq "" goto SkipSync

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

:SkipSync

echo @@@BUILD_STEP gyp_compile@@@
call vcvarsall.bat x86 && call devenv.com build\all.sln /build %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP gyp_tests@@@
python_slave.exe trusted_test.py --config %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo on
echo @@@BUILD_STEP scons_compile@@@
call vcvarsall.bat %VCBITS% && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP small_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc small_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP medium_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc medium_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP large_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc large_tests platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

if "%INSIDE_TOOLCHAIN%" neq "" goto SkipArchive

echo @@@BUILD_STEP chrome_browser_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP pyauto_tests@@@
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 -k --verbose --mode=%MODE%-win,nacl,doc SILENT=1 platform=x86-%BITS% ^
 pyauto_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)

if %BUILDBOT_BUILDERNAME% equ vista64-m64-n64-dbg goto ArchiveIt
if %BUILDBOT_BUILDERNAME% equ vista64-m64-n64-opt goto ArchiveIt
goto SkipArchive
:ArchiveIt
echo @@@BUILD_STEP archive_build@@@
set OLD_PATH=%PATH%
set PATH=c:\cygwin\bin;%PATH%
tar cvfz build.tgz scons-out/
set PATH=%OLD_PATH%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
set GS_BASE="gs://nativeclient-archive2/between_builders/vista64_%MODE%"
set HOME=c:\Users\chrome-bot
\b\build\scripts\slave\gsutil -h Cache-Control:no-cache ^
 cp -a public-read build.tgz ^
 %GS_BASE%/rev_%BUILDBOT_GOT_REVISION%/build.tgz
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
:SkipArchive

exit /b %RETCODE%
