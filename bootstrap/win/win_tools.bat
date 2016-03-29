@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This script will try to find if svn and python are accessible and it not,
:: it will try to download it and 'install' it in depot_tools.

:: Sadly, we can't use SETLOCAL here otherwise it ERRORLEVEL is not correctly
:: returned.

set WIN_TOOLS_ROOT_URL=https://src.chromium.org/svn/trunk/tools
:: It used to be %~dp0 but ADODB.Stream may fail to write to this directory if
:: the directory DACL is set to elevated integrity level.
set ZIP_DIR=%TEMP%

:: Get absolute root directory (.js scripts don't handle relative paths well).
pushd %~dp0..\..
set WIN_TOOLS_ROOT_DIR=%CD%
popd

if "%1" == "force" (
  set WIN_TOOLS_FORCE=1
  shift /1
)


:PYTHON_CHECK
if not exist "%WIN_TOOLS_ROOT_DIR%\python276_bin" goto :PY27_INSTALL
if not exist "%WIN_TOOLS_ROOT_DIR%\python.bat" goto :PY27_INSTALL
set ERRORLEVEL=0
goto :GIT_CHECK


:PY27_INSTALL
echo Installing python 2.7.6...
:: Cleanup python directory if it was existing.
if exist "%WIN_TOOLS_ROOT_DIR%\python276_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\python276_bin"
if exist "%ZIP_DIR%\python276.zip" del "%ZIP_DIR%\python276.zip"
echo Fetching from %WIN_TOOLS_ROOT_URL%/third_party/python276_bin.zip
cscript //nologo //e:jscript "%~dp0get_file.js" %WIN_TOOLS_ROOT_URL%/third_party/python276_bin.zip "%ZIP_DIR%\python276_bin.zip"
if errorlevel 1 goto :PYTHON_FAIL
:: Will create python276_bin\...
cscript //nologo //e:jscript "%~dp0unzip.js" "%ZIP_DIR%\python276_bin.zip" "%WIN_TOOLS_ROOT_DIR%"
:: Create the batch files.
call copy /y "%~dp0python276.new.bat" "%WIN_TOOLS_ROOT_DIR%\python.bat" 1>nul
call copy /y "%~dp0pylint.new.bat" "%WIN_TOOLS_ROOT_DIR%\pylint.bat" 1>nul
del "%ZIP_DIR%\python276_bin.zip"
set ERRORLEVEL=0
goto :GIT_CHECK


:PYTHON_FAIL
echo ... Failed to checkout python automatically.
echo You should get the "prebaked" version at %WIN_TOOLS_ROOT_URL%/third_party/
set ERRORLEVEL=1
goto :END

:GIT_CHECK

:: See if we're on XP
for /f "tokens=2 delims=[]" %%i in ('ver') do set VERSTR=%%i
for /f "tokens=2,3 delims=. " %%i in ("%VERSTR%") do (set VERMAJOR=%%i & set VERMINOR=%%j)
if %VERMAJOR% lss 5 set GIT_VERSION=%GIT_VERSION%-xp
if %VERMAJOR% equ 5 if %VERMINOR% lss 2 set XP_SUFFIX=-xp

:: Check to see if we're on a 32 or 64 bit system
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set OS_BITS=32 || set OS_BITS=64

if not exist "%WIN_TOOLS_ROOT_DIR%\.git_bleeding_edge" goto :GIT_OLD_FLOW
set GIT_PORTABLE_FLOW=1
set GIT_VERSION=2.7.4-%OS_BITS%
set GIT_FETCH_URL=https://commondatastorage.googleapis.com/chrome-infra/PortableGit-%GIT_VERSION%-bit.7z.exe
set GIT_DOWNLOAD_PATH=%ZIP_DIR%\git.7z.exe
set GIT_BIN_DIR=git-%GIT_VERSION%_bin
set GIT_INST_DIR=%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%
set GIT_EXE_PATH=%GIT_INST_DIR%\bin\git.exe
goto :GIT_CLEANUP

:GIT_OLD_FLOW
set GIT_VERSION=1.9.5.chromium.6%XP_SUFFIX%
set GIT_BIN_DIR=git-%GIT_VERSION%_bin
set GIT_FETCH_URL=https://commondatastorage.googleapis.com/chrome-infra/%GIT_BIN_DIR%.zip
set GIT_DOWNLOAD_PATH=%ZIP_DIR%\git.zip
set GIT_INST_DIR=%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%
set GIT_EXE_PATH=%GIT_INST_DIR%\cmd\git.cmd

:GIT_CLEANUP
:: Clean up any release which doesn't match the one we want.
for /d %%i in ("%WIN_TOOLS_ROOT_DIR%\git-*_bin") do (
  if not "%%i" == "%WIN_TOOLS_ROOT_DIR%\git-%GIT_VERSION%_bin" (
    echo Cleaning old git installation %%i
    rmdir /s /q "%%i"
  )
)

if "%WIN_TOOLS_FORCE%" == "1" goto :GIT_INSTALL

if exist "%GIT_EXE_PATH%" (
  call "%GIT_EXE_PATH%" --version 2>nul 1>nul
  if errorlevel 1 goto :GIT_INSTALL
  rem Several git versions can live side-by-side; check the top-level
  rem batch script to make sure it points to the desired version.
  find "%GIT_BIN_DIR%" "%WIN_TOOLS_ROOT_DIR%\git.bat" 2>nul 1>nul
  if errorlevel 1 goto :GIT_MAKE_BATCH_FILES
  goto :SVN_CHECK
)
goto :GIT_INSTALL

:GIT_INSTALL
echo Installing git %GIT_VERSION% (avg 1-2 min download) ...
echo Detected: %OS_BITS% bits
if defined XP_SUFFIX         echo ... Windows XP
if defined GIT_PORTABLE_FLOW echo ... Portable installation flow

:: git is not accessible; check it out and create 'proxy' files.
if exist "%GIT_DOWNLOAD_PATH%" del "%GIT_DOWNLOAD_PATH%"
echo Fetching from %GIT_FETCH_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %GIT_FETCH_URL% "%GIT_DOWNLOAD_PATH%"
if errorlevel 1 goto :GIT_FAIL
:: Cleanup git directory if it already exists.
if exist "%GIT_INST_DIR%\." rd /q /s "%GIT_INST_DIR%"

if defined GIT_PORTABLE_FLOW (
  rem run PortableGit self-extractor
  rem -y : Be Quiet ("yes")
  rem -sd1 : Self delete SFX archive
  rem -InstallPath : Where to put the files
  rem -Directory : Run the post-extract program with this current-working-directory
  call "%GIT_DOWNLOAD_PATH%" -y -sd1 -InstallPath="%GIT_INST_DIR%" -Directory="%GIT_INST_DIR%"
) else (
  rem Will create %GIT_INST_DIR%\...
  cscript //nologo //e:jscript "%~dp0unzip.js" "%GIT_DOWNLOAD_PATH%" "%WIN_TOOLS_ROOT_DIR%"
)

if errorlevel 1 goto :GIT_FAIL
del "%GIT_DOWNLOAD_PATH%"
if not exist "%GIT_INST_DIR%\." goto :GIT_FAIL

:GIT_MAKE_BATCH_FILES
:: Create the batch files.
set GIT_TEMPL=%~dp0git.template.bat
set SED=%GIT_INST_DIR%\usr\bin\sed.exe
if defined GIT_PORTABLE_FLOW (
  rem turns out we just installed sed :)
  call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/" -e "s/GIT_PROGRAM/cmd\\\\git.exe/" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\git.bat"
  call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/" -e "s/GIT_PROGRAM/cmd\\\\gitk.exe/" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\gitk.bat"
  call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/" -e "s/GIT_PROGRAM/usr\\\\bin\\\\ssh.exe/" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\ssh.bat"
  call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/" -e "s/GIT_PROGRAM/usr\\\\bin\\\\ssh-keygen.exe/" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\ssh-keygen.bat"
) else (
  call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\git.bat" "%WIN_TOOLS_ROOT_DIR%\git.bat" 1>nul
  call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\gitk.bat" "%WIN_TOOLS_ROOT_DIR%\gitk.bat" 1>nul
  call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\ssh.bat" "%WIN_TOOLS_ROOT_DIR%\ssh.bat" 1>nul
  call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\ssh-keygen.bat" "%WIN_TOOLS_ROOT_DIR%\ssh-keygen.bat" 1>nul
)

:: Ensure autocrlf and filemode are set correctly.
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.autocrlf false
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.filemode false
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.preloadindex true
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.fscache true
goto :SVN_CHECK


:GIT_FAIL
echo ... Failed to checkout git automatically.
echo You should get the "prebaked" version used at %GIT_FETCH_URL%
set ERRORLEVEL=1
goto :END


:SVN_CHECK
:: If the batch file exists, skip the svn check.
if exist "%WIN_TOOLS_ROOT_DIR%\svn.bat" goto :END
if "%WIN_TOOLS_FORCE%" == "1" goto :SVN_INSTALL
call svn --version 2>nul 1>nul
if errorlevel 1 goto :SVN_INSTALL
goto :END


:SVN_INSTALL
echo Installing subversion ...
:: svn is not accessible; check it out and create 'proxy' files.
if exist "%ZIP_DIR%\svn.zip" del "%ZIP_DIR%\svn.zip"
echo Fetching from %WIN_TOOLS_ROOT_URL%/third_party/svn_bin.zip
cscript //nologo //e:jscript "%~dp0get_file.js" %WIN_TOOLS_ROOT_URL%/third_party/svn_bin.zip "%ZIP_DIR%\svn.zip"
if errorlevel 1 goto :SVN_FAIL
:: Cleanup svn directory if it was existing.
if exist "%WIN_TOOLS_ROOT_DIR%\svn\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\svn"
if exist "%WIN_TOOLS_ROOT_DIR%\svn_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\svn_bin"
:: Will create svn_bin\...
cscript //nologo //e:jscript "%~dp0unzip.js" "%ZIP_DIR%\svn.zip" "%WIN_TOOLS_ROOT_DIR%"
if errorlevel 1 goto :SVN_FAIL
if not exist "%WIN_TOOLS_ROOT_DIR%\svn_bin\." goto :SVN_FAIL
del "%ZIP_DIR%\svn.zip"
:: Create the batch file.
call copy /y "%~dp0svn.new.bat" "%WIN_TOOLS_ROOT_DIR%\svn.bat" 1>nul
call copy /y "%~dp0svnversion.new.bat" "%WIN_TOOLS_ROOT_DIR%\svnversion.bat" 1>nul
goto :END


:SVN_FAIL
echo ... Failed to checkout svn automatically.
echo You should get the "prebaked" version at %WIN_TOOLS_ROOT_URL%/third_party/
set ERRORLEVEL=1
goto :END


:returncode
set WIN_TOOLS_ROOT_URL=
set WIN_TOOLS_ROOT_DIR=
exit /b %ERRORLEVEL%

:END
call :returncode %ERRORLEVEL%
