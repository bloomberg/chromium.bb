@echo off
setlocal

call python "%~dp0cpplint.py" %*
