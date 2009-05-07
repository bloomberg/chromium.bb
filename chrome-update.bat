@echo off

:: This batch file assumes that the correct version of python can be found in
:: the current directory, and that you have Visual Studio 8 installed in the
:: default location.

setlocal
call vcvars32.bat

python "%~dp0chrome-update.py" %*
