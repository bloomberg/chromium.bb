@ECHO OFF
REM Copyright (c) 2011 The Chromium Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
REM
REM Purpose of this file: If you have IE9 on your machine, not all of
REM the Chrome Frame tests will currently pass. To run the tests on
REM a VM or separate machine, you need to copy a bunch of things over
REM (the tests run slowly or flakily over a network drive).
REM
REM To facilitate running the tests on a separate machine, Run this
REM batch file on a VM or a separate machine, that has a drive mapped
REM to your dev machine (so that it can access your dev workspace).
REM
REM Before running this file, you need to build all the chrome_frame
REM targets plus chrome.dll and chrome.exe on your dev machine. Run
REM the copy of this file that exists in the same workspace that you
REM want to test.
REM
REM NOTE: I've seen cases where a Vista VM under Virtual PC will fail
REM to copy all of the files from the dev machine, and doing e.g.
REM [ dir j:\src\chrome\src\chrome\debug ] (assuming j:\src\chrome\src
REM is your workspace on the dev machine) shows an empty filing listing
REM when run from the VM, whereas your workspace actually just built
REM a ton of stuff in that directory. Just be aware of it, and check
REM what files you actually have in the local copy on the VM after
REM running this script. If you encounter this issue, a reboot of the
REM VM seems to help.

setlocal

REM Get the path to the build tree's src directory.
CALL :_canonicalize "%~dp0..\.."
SET FROM=%RET%

REM Read OUTPUT and/or BUILDTYPE from command line.
FOR %%a IN (%1 %2) do (
IF "%%a"=="out" SET OUTPUT=out
IF "%%a"=="build" SET OUTPUT=build
IF "%%a"=="Debug" SET BUILDTYPE=Debug
IF "%%a"=="Release" SET BUILDTYPE=Release
)

CALL :_find_build
IF "%OUTPUT%%BUILDTYPE%"=="" (
ECHO No build found to copy.
EXIT 1
)

set CLIENT_ROOT=%FROM%
SET INSTALL_ROOT=\trybot

@ECHO ON
IF NOT EXIST "%INSTALL_ROOT%" mkdir "%INSTALL_ROOT%"
cd %INSTALL_ROOT%
rmdir /s /q base
rmdir /s /q %OUTPUT%\%BUILDTYPE%
rmdir /s /q chrome_frame
mkdir base
mkdir %OUTPUT%\%BUILDTYPE%
mkdir chrome_frame\test\data
mkdir chrome_frame\test\html_util_test_data
mkdir net\data
mkdir net\tools\testserver
mkdir third_party\pyftpdlib
mkdir third_party\pylib
mkdir third_party\python_26
mkdir third_party\pywebsocket
mkdir third_party\tlslite
copy %CLIENT_ROOT%\base\base_paths_win.cc base\base_paths_win.cc
xcopy %CLIENT_ROOT%\%OUTPUT%\%BUILDTYPE% %OUTPUT%\%BUILDTYPE% /E /EXCLUDE:%CLIENT_ROOT%\chrome_frame\test\poor_mans_trybot_xcopy_filter.txt
xcopy %CLIENT_ROOT%\chrome_frame\test\data chrome_frame\test\data /E
xcopy %CLIENT_ROOT%\net\data net\data /E
xcopy %CLIENT_ROOT%\net\tools\testserver net\tools\testserver /E
xcopy %CLIENT_ROOT%\third_party\pyftpdlib third_party\pyftpdlib /E
xcopy %CLIENT_ROOT%\third_party\pylib third_party\pylib /E
xcopy %CLIENT_ROOT%\third_party\python_26 third_party\python_26 /E
xcopy %CLIENT_ROOT%\third_party\pywebsocket third_party\pywebsocket /E
xcopy %CLIENT_ROOT%\third_party\tlslite third_party\tlslite /E
xcopy %CLIENT_ROOT%\chrome_frame\test\html_util_test_data chrome_frame\test\html_util_test_data /E
copy %CLIENT_ROOT%\chrome_frame\CFInstance.js chrome_frame\CFInstance.js
copy %CLIENT_ROOT%\chrome_frame\CFInstall.js chrome_frame\CFInstall.js
@ECHO OFF
echo ************************************
echo DO THE FOLLOWING IN AN ADMIN PROMPT:
echo *********************************
echo %INSTALL_ROOT%\%OUTPUT%\%BUILDTYPE%\chrome_frame_unittests.exe
echo %INSTALL_ROOT%\%OUTPUT%\%BUILDTYPE%\chrome_frame_tests.exe
echo %INSTALL_ROOT%\%OUTPUT%\%BUILDTYPE%\chrome_frame_net_tests.exe
goto end

:usage
echo "Usage: poor_mans_trybot.bat [out|build] [Debug|Release]"

:end
GOTO :EOF

REM All labels henceforth are subroutines intended to be invoked by CALL.

REM Canonicalize the first argument, returning it in RET.
:_canonicalize
SET RET=%~f1
GOTO :EOF

REM Search for a npchrome_frame.dll in the candidate build outputs.
:_find_build
IF "%OUTPUT%"=="" (
SET OUTPUTS=out build
) ELSE (
SET OUTPUTS=%OUTPUT%
SET OUTPUT=
)

IF "%BUILDTYPE%"=="" (
SET BUILDTYPES=Debug Release
) ELSE (
SET BUILDTYPES=%BUILDTYPE%
SET BUILDTYPE=
)

FOR %%o IN (%OUTPUTS%) DO (
FOR %%f IN (%BUILDTYPES%) DO (
IF EXIST "%FROM%\%%o\%%f\npchrome_frame.dll" (
SET OUTPUT=%%o
SET BUILDTYPE=%%f
GOTO :EOF
)
)
)
GOTO :EOF
