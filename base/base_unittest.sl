; This file is needed for the experimental Slavelastic/Swarm project.
; Talk to csharp@chromium.org for details
; base_unittests.sl: Slavelastic Manifest file for Base Unit Tests
 
[Header]
name = Base Unittests

[Windows]
executable = src\build\%(target)s\base_unittests.exe

[Linux]
executable = src/out/%(target)s/base_unittests

[Mac]
; PLACEHOLDER ONLY, WON'T WORK
executable = src/out/%(target)s/base_unittests