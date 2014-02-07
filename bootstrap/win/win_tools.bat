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


:GIT_CHECK
if "%DEPOT_TOOLS_GIT_1852%" == "1" goto :GIT_1852_CHECK
goto :GIT_180_CHECK


:GIT_1852_CHECK
set GIT_VERSION=1.8.5.2.chromium.1
set GIT_BIN_DIR=git-%GIT_VERSION%_bin
set GIT_ZIP_FILE=%GIT_BIN_DIR%.zip
set GIT_ZIP_URL=http://commondatastorage.googleapis.com/chrome-infra/%GIT_ZIP_FILE%
goto :GIT_COMMON


:GIT_180_CHECK
set GIT_VERSION=1.8.0
set GIT_BIN_DIR=git-%GIT_VERSION%_bin
set GIT_ZIP_FILE=%GIT_BIN_DIR%.zip
set GIT_ZIP_URL=%WIN_TOOLS_ROOT_URL%/third_party/%GIT_ZIP_FILE%
goto :GIT_COMMON


:GIT_COMMON
:: If the batch file exists, skip the git check.
if exist "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%" goto :MSYS_PATH_CHECK
if "%CHROME_HEADLESS%" == "1" goto :SVN_CHECK
if "%WIN_TOOLS_FORCE%" == "1" goto :GIT_INSTALL
call git --version 2>nul 1>nul
if errorlevel 1 goto :GIT_INSTALL
goto :SVN_CHECK


:MSYS_PATH_CHECK
call find "mingw" "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\cmd\git.cmd" 2>nul 1>nul
if errorlevel 1 goto :SVN_CHECK
rmdir /S /Q "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%"


:GIT_INSTALL
echo Installing git %GIT_VERSION% (avg 1-2 min download) ...
:: git is not accessible; check it out and create 'proxy' files.
if exist "%ZIP_DIR%\git.zip" del "%ZIP_DIR%\git.zip"
echo Fetching from %GIT_ZIP_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %GIT_ZIP_URL% "%ZIP_DIR%\git.zip"
if errorlevel 1 goto :GIT_FAIL
:: Cleanup svn directory if it was existing.
if exist "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%"
:: Will create %GIT_BIN_DIR%\...
cscript //nologo //e:jscript "%~dp0unzip.js" "%ZIP_DIR%\git.zip" "%WIN_TOOLS_ROOT_DIR%"
if errorlevel 1 goto :GIT_FAIL
if not exist "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\." goto :GIT_FAIL
del "%ZIP_DIR%\git.zip"
:: Create the batch files.
call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\git.bat" "%WIN_TOOLS_ROOT_DIR%\git.bat" 1>nul
call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\gitk.bat" "%WIN_TOOLS_ROOT_DIR%\gitk.bat" 1>nul
call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\ssh.bat" "%WIN_TOOLS_ROOT_DIR%\ssh.bat" 1>nul
call copy /y "%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%\ssh-keygen.bat" "%WIN_TOOLS_ROOT_DIR%\ssh-keygen.bat" 1>nul
:: Ensure autocrlf and filemode are set correctly.
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --global core.autocrlf false
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --global core.filemode false
goto :SVN_CHECK


:GIT_FAIL
echo ... Failed to checkout git automatically.
echo Please visit http://code.google.com/p/msysgit to download the latest git
echo client before continuing.
echo You can also get the "prebaked" version used at %GIT_ZIP_URL%
set ERRORLEVEL=1
goto :END


:SVN_CHECK
:: If the batch file exists, skip the svn check.
if exist "%WIN_TOOLS_ROOT_DIR%\svn.bat" goto :PYTHON_CHECK
if "%WIN_TOOLS_FORCE%" == "1" goto :SVN_INSTALL
call svn --version 2>nul 1>nul
if errorlevel 1 goto :SVN_INSTALL
goto :PYTHON_CHECK


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
goto :PYTHON_CHECK


:SVN_FAIL
echo ... Failed to checkout svn automatically.
echo Please visit http://subversion.tigris.org to download the latest subversion client
echo before continuing.
echo You can also get the "prebacked" version used at %WIN_TOOLS_ROOT_URL%/third_party/
set ERRORLEVEL=1
goto :END


:PYTHON_CHECK
:: Note: while the variable talks about 2.7.5, we are now installing 2.7.6.
:: Sorry for the confusion. :(
if "%DEPOT_TOOLS_PYTHON_275%" == "0" goto :PY26_CHECK
if "%DEPOT_TOOLS_PYTHON_27%" == "0" goto :PY26_CHECK
goto :PY27_CHECK


:PY26_CHECK
if not exist "%WIN_TOOLS_ROOT_DIR%\python_bin" goto :PY26_INSTALL
if not exist "%WIN_TOOLS_ROOT_DIR%\python.bat" goto :PY26_INSTALL
set ERRORLEVEL=0
goto :END


:PY27_CHECK
if not exist "%WIN_TOOLS_ROOT_DIR%\python276_bin" goto :PY27_INSTALL
if not exist "%WIN_TOOLS_ROOT_DIR%\python.bat" goto :PY27_INSTALL
set ERRORLEVEL=0
goto :END


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
goto :END


:PY26_INSTALL
echo Installing python 2.6...
:: Cleanup python directory if it was existing.
if exist "%WIN_TOOLS_ROOT_DIR%\python_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\python_bin"
call svn co -q %WIN_TOOLS_ROOT_URL%/third_party/python_26 "%WIN_TOOLS_ROOT_DIR%\python_bin"
if errorlevel 1 goto :PYTHON_FAIL
:: Create the batch files.
call copy /y "%~dp0python.new.bat" "%WIN_TOOLS_ROOT_DIR%\python.bat" 1>nul
call copy /y "%~dp0pylint.new.bat" "%WIN_TOOLS_ROOT_DIR%\pylint.bat" 1>nul
set ERRORLEVEL=0
goto :END


:PYTHON_FAIL
echo ... Failed to checkout python automatically.
echo Please visit http://python.org to download the latest python 2.7.x client before
echo continuing.
echo You can also get the "prebacked" version used at %WIN_TOOLS_ROOT_URL%/third_party/
set ERRORLEVEL=1
goto :END


:returncode
set WIN_TOOLS_ROOT_URL=
set WIN_TOOLS_ROOT_DIR=
exit /b %ERRORLEVEL%

:END
call :returncode %ERRORLEVEL%
