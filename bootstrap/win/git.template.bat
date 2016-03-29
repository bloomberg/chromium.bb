@echo off
setlocal
if not defined EDITOR set EDITOR=notepad
set PATH=%~dp0GIT_BIN_DIR\cmd;%PATH%
"%~dp0GIT_BIN_DIR\GIT_PROGRAM" %*
