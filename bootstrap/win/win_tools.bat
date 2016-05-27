@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This script will determine if python, git, or svn binaries need updates.  It
:: returns 123 if the user's shell must restart, otherwise !0 is failure

:: Sadly, we can't use SETLOCAL here otherwise it ERRORLEVEL is not correctly
:: returned.

set CHROME_INFRA_URL=https://storage.googleapis.com/chrome-infra/
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
set PYTHON_URL=%CHROME_INFRA_URL%python276_bin.zip
if exist "%WIN_TOOLS_ROOT_DIR%\python276_bin\." rd /q /s "%WIN_TOOLS_ROOT_DIR%\python276_bin"
if exist "%ZIP_DIR%\python276.zip" del "%ZIP_DIR%\python276.zip"
echo Fetching from %PYTHON_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %PYTHON_URL% "%ZIP_DIR%\python276_bin.zip"
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
echo You should get the "prebaked" version at %PYTHON_URL%
set ERRORLEVEL=1
goto :END

:GIT_CHECK

:: must explicitly use FIND_EXE to prevent this from grabbing e.g. gnuwin32 or
:: msys versions.
set FIND_EXE=%SYSTEMROOT%\System32\find.exe

:: Check to see if we're on a 32 or 64 bit system
:: (parens) are necessary, otherwise batch puts an extra space after 32.
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | %FIND_EXE% /i "x86" > NUL && (set OS_BITS=32) || (set OS_BITS=64)

if not exist "%WIN_TOOLS_ROOT_DIR%\.git_bleeding_edge" (
  set GIT_VERSION=2.8.3
) else (
  set GIT_VERSION=2.8.3
)
set GIT_VERSION=%GIT_VERSION%-%OS_BITS%

set GIT_FETCH_URL=%CHROME_INFRA_URL%PortableGit-%GIT_VERSION%-bit.7z.exe
set GIT_DOWNLOAD_PATH=%ZIP_DIR%\git.7z.exe
set GIT_BIN_DIR=git-%GIT_VERSION%_bin
set GIT_INST_DIR=%WIN_TOOLS_ROOT_DIR%\%GIT_BIN_DIR%
set GIT_EXE_PATH=%GIT_INST_DIR%\bin\git.exe

:: Clean up any release which doesn't match the one we want.
for /d %%i in ("%WIN_TOOLS_ROOT_DIR%\git-*_bin") do (
  if not "%%i" == "%WIN_TOOLS_ROOT_DIR%\git-%GIT_VERSION%_bin" (
    echo Cleaning old git installation %%i
    rmdir /s /q "%%i" > NUL
  )
)

if "%WIN_TOOLS_FORCE%" == "1" goto :GIT_INSTALL

if not exist "%GIT_EXE_PATH%" goto :GIT_INSTALL

call "%GIT_EXE_PATH%" --version 2>nul 1>nul
if errorlevel 1 goto :GIT_INSTALL

:: Several git versions can live side-by-side; check the top-level
:: batch script to make sure it points to the desired version.
for %%f in (git.bat gitk.bat ssh.bat ssh-keygen.bat git-bash) do (
  %FIND_EXE% "%GIT_BIN_DIR%" "%WIN_TOOLS_ROOT_DIR%\%%f" 2>nul 1>nul
  if errorlevel 1 goto :GIT_MAKE_BATCH_FILES
)
if not exist %GIT_INST_DIR%\etc\profile.d\python.sh goto :GIT_MAKE_BATCH_FILES
goto :SYNC_GIT_HELP_FILES

:GIT_INSTALL
echo Installing git %GIT_VERSION% (avg 1-2 min download) ...

if exist "%GIT_DOWNLOAD_PATH%" del "%GIT_DOWNLOAD_PATH%"
echo Fetching from %GIT_FETCH_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %GIT_FETCH_URL% "%GIT_DOWNLOAD_PATH%"
if errorlevel 1 goto :GIT_FAIL
:: Cleanup git directory if it already exists.
if exist "%GIT_INST_DIR%\." rd /q /s "%GIT_INST_DIR%"

:: run PortableGit self-extractor
:: -y : Be Quiet ("yes")
:: -sd1 : Self delete SFX archive
:: -InstallPath : Where to put the files
:: -Directory : Run the post-extract program with this current-working-directory
::
:: Path slashes must be escaped or the 7zip sfx treats e.g. path\to\dir as
::   path[tab]o\dir.
set GIT_INST_DIR_ESC=%GIT_INST_DIR:\=\\%
call "%GIT_DOWNLOAD_PATH%" -y -sd1 -InstallPath="%GIT_INST_DIR_ESC%" -Directory="%GIT_INST_DIR_ESC%"
if errorlevel 1 goto :GIT_FAIL

del "%GIT_DOWNLOAD_PATH%"
if not exist "%GIT_INST_DIR%\." goto :GIT_FAIL

set DID_UPGRADE=1

:GIT_MAKE_BATCH_FILES
:: Create the batch files.
set GIT_TEMPL=%~dp0git.template.bat
set SED=%GIT_INST_DIR%\usr\bin\sed.exe
call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/g" -e "s/GIT_PROGRAM/cmd\\\\git.exe/g" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\git.bat"
call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/g" -e "s/GIT_PROGRAM/cmd\\\\gitk.exe/g" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\gitk.bat"
call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/g" -e "s/GIT_PROGRAM/usr\\\\bin\\\\ssh.exe/g" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\ssh.bat"
call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/g" -e "s/GIT_PROGRAM/usr\\\\bin\\\\ssh-keygen.exe/g" < %GIT_TEMPL% > "%WIN_TOOLS_ROOT_DIR%\ssh-keygen.bat"
call "%SED%" -e "s/GIT_BIN_DIR/%GIT_BIN_DIR%/g" -e "s/PYTHON_BIN_DIR/python276_bin/g" -e "s/SVN_BIN_DIR/svn_bin/g" < %~dp0git-bash.template.sh > "%WIN_TOOLS_ROOT_DIR%\git-bash"
copy "%~dp0profile.d.python.sh" %GIT_INST_DIR%\etc\profile.d\python.sh > NUL

:: Ensure various git configurations are set correctly at they system level.
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.autocrlf false
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.filemode false
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.preloadindex true
call "%WIN_TOOLS_ROOT_DIR%\git.bat" config --system core.fscache true

:SYNC_GIT_HELP_FILES
:: Copy all the depot_tools docs into the mingw{bits} git docs root.
:: /i : Make sure xcopy knows that the destination names a folder, not a file
:: /q : Make xcopy quiet (though it still prints a `X File(s) copied` message
::      which is why we have the > NUL)
:: /d : Copy source files that are newer than the corresponding destination
::      files only. This prevents excessive copying when none of the docs
::      actually changed.
:: /y : Don't prompt for overwrites (yes)
xcopy /i /q /d /y "%WIN_TOOLS_ROOT_DIR%\man\html\*" "%GIT_INST_DIR%\mingw%OS_BITS%\share\doc\git-doc" > NUL

:: MSYS users need to restart their shell.
if defined MSYSTEM if defined DID_UPGRADE (
  echo.
  echo.
  echo [1;31mIMPORTANT:[0m
  echo depot_tools' git distribution has been updated while inside of a MinGW
  echo shell. In order to complete the upgrade, please exit the shell and re-run
  echo `git bash` from cmd.
  exit 123
)

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
set SVN_URL=%CHROME_INFRA_URL%svn_bin.zip
if exist "%ZIP_DIR%\svn.zip" del "%ZIP_DIR%\svn.zip"
echo Fetching from %SVN_URL%
cscript //nologo //e:jscript "%~dp0get_file.js" %SVN_URL% "%ZIP_DIR%\svn.zip"
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
echo You should get the "prebaked" version at %SVN_URL%
set ERRORLEVEL=1
goto :END


:returncode
set WIN_TOOLS_ROOT_DIR=
exit /b %ERRORLEVEL%

:END
call :returncode %ERRORLEVEL%
