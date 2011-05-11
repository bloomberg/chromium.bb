:: Copyright (c) 2011 The Native Client Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

echo on

setlocal
set MODE=%1
set BITS=%2
set TOOLCHAIN=%3

:: Picking out drive letter on which the build is happening so we can use it
:: for the temp directory.
set BUILD_DRIVE=%PATH:~0,1%

call buildbot\msvs_env.bat %BITS%

set RETCODE=0

if "%MODE%" equ "dbg" (set GYPMODE=Debug) else (set GYPMODE=Release)
if %BITS% equ 32 (set VCBITS=x86) else (set VCBITS=x64)
if %BITS% equ 32 (set SEL_LDR=sel_ldr.exe) else (set SEL_LDR=sel_ldr64.exe)
if "%TOOLCHAIN%" equ "glibc" (set GLIBCOPTS=--nacl_glibc) else (set GLIBCOPTS=)

:: Skip over hooks, clobber, and partial_sdk when run inside the toolchain
:: build as the toolchain takes care or the clobber, hooks aren't needed, and
:: partial_sdk really shouldn't be needed.
if "%INSIDE_TOOLCHAIN%" neq "" goto SkipSync

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
:SkipClean

echo @@@BUILD_STEP gclient_runhooks@@@
cmd /c gclient runhooks
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo @@@BUILD_STEP clobber@@@
rd /s /q scons-out ^
 & rd /s /q build\Debug build\Release ^
 & rd /s /q build\Debug-Win32 build\Release-Win32 ^
 & rd /s /q build\Debug-x64 build\Release-x64

echo @@@BUILD_STEP partial_sdk@@@
if "%TOOLCHAIN%" equ "glibc" (
  call scons.bat --verbose --mode=nacl_extra_sdk platform=x86-%BITS% ^
    --download --nacl_glibc extra_sdk_update_header extra_sdk_update
  if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

  if %BITS% equ 32 goto SkipSync
  echo @@@BUILD_STEP partial_sdk_32@@@
  setlocal
  call vcvarsall.bat x86 && call scons.bat --verbose --mode=nacl_extra_sdk -j 8 ^
    %GLIBCOPTS% platform=x86-32 extra_sdk_update_header extra_sdk_update
  if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
  endlocal
) else (
  call scons.bat --verbose --mode=nacl_extra_sdk platform=x86-%BITS% ^
    --download extra_sdk_update_header install_libpthread extra_sdk_update
  if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

  if %BITS% equ 32 goto SkipSync
  echo @@@BUILD_STEP partial_sdk_32@@@
  setlocal
  call vcvarsall.bat x86 && call scons.bat --verbose --mode=nacl_extra_sdk -j 8 ^
    platform=x86-32 extra_sdk_update_header install_libpthread extra_sdk_update
  if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
  endlocal
)
:SkipSync

echo @@@BUILD_STEP gyp_compile@@@
setlocal
call vcvarsall.bat x86 && call devenv.com build\all.sln /build %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal

echo @@@BUILD_STEP gyp_tests@@@
python_slave.exe trusted_test.py --config %GYPMODE%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo on
echo @@@BUILD_STEP scons_compile@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc platform=x86-%BITS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal

if "%TOOLCHAIN%" equ "glibc" goto NoNeedForPlugin
echo @@@BUILD_STEP plugin_compile@@@
setlocal
call vcvarsall.bat x86 && call scons.bat -j 8 ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc platform=x86-32 plugin
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
endlocal
:NoNeedForPlugin

echo @@@BUILD_STEP small_tests@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc small_tests ^
 platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

:: TODO(bradchen): add dynamic_library_browser_tests
::  when DSOs are added to Windows toolchain build

if "%TOOLCHAIN%" equ "glibc" goto SkipNonGlibsTests
echo @@@BUILD_STEP medium_tests@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc medium_tests ^
 platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

echo @@@BUILD_STEP large_tests@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc large_tests ^
 platform=x86-%BITS%
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

if "%INSIDE_TOOLCHAIN%" neq "" goto SkipArchive

echo @@@BUILD_STEP chrome_browser_tests@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc ^
 SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

echo @@@BUILD_STEP chrome_browser_tests using GYP@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc ^
 SILENT=1 platform=x86-%BITS% ^
 force_ppapi_plugin=build\%GYPMODE%\ppGoogleNaClPlugin.dll ^
 force_sel_ldr=build\%GYPMODE%\%SEL_LDR% ^
 chrome_browser_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal


# TODO(mseaborn): Drop support for non-IRT builds so that this is the
# default.  See http://code.google.com/p/nativeclient/issues/detail?id=1691
echo @@@BUILD_STEP chrome_browser_tests using IRT@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc ^
 SILENT=1 platform=x86-%BITS% ^
 chrome_browser_tests irt=1
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

echo @@@BUILD_STEP pyauto_tests@@@
setlocal
call vcvarsall.bat %VCBITS% && call scons.bat ^
 DOXYGEN=..\third_party\doxygen\win\doxygen ^
 %GLIBCOPTS% -k --verbose --mode=%MODE%-win,nacl,doc ^
 SILENT=1 platform=x86-%BITS% ^
 pyauto_tests
if %ERRORLEVEL% neq 0 (set RETCODE=%ERRORLEVEL% & echo @@@STEP_FAILURE@@@)
endlocal

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
:SkipNonGlibsTests

if %RETCODE% neq 0 (
  echo @@@BUILD_STEP summary@@@
  echo There were failed stages.
  exit /b %RETCODE%
)
