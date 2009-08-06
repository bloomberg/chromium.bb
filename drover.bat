@echo off

setlocal
set PATH=%~dp0svn;%PATH%

call python "%~dp0drover.py" %*