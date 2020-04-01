@echo off
setlocal
if not defined EDITOR set EDITOR=notepad
set PATH=%~dp0${GIT_BIN_RELDIR}\cmd;%~dp0;%PATH%

REM This hack tries to restore as many original arguments as possible
REM "HEAD^^1" is parsed correctly, but "HEAD^1" is still not.
REM TODO(crbug.com/1066663): Fix passing "HEAD^1" as argument.
call :RunGit "%*"
REM exit /b from call :RunGit won't be sent to the prompt.
if %ERRORLEVEL% NEQ 0 (
  exit /b %ERRORLEVEL%
)
goto :eof

:RunGit
call "%~dp0${GIT_BIN_RELDIR}\${GIT_PROGRAM}" %~1
if %ERRORLEVEL% NEQ 0 (
  exit /b %ERRORLEVEL%
)
