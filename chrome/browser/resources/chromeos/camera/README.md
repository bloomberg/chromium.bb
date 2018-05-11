Camera App
==========

Camera App is a packaged app designed to take pictures with several effects using the embedded web camera.

Supported systems
-----------------
Should work on any operating system, especially on Chrome OS.

Compiling and installing
------------------------

To compile run "make all", then drag the camera.crx package from the build directory to Chrome. Note, that currently building on Linux only is supported. However, the crx packages should work on any other operating system without problems.

Automated tests
---------------

To perform automated tests on Linux, run "make run-tests". Note, that these tests are experimental.
The script must be able to execute sudo without a password.

The automated tests run chromium with a testing package of the Camera app. The Camera app communicates with the python test runner via web sockets. Note, that you will need a usb web camera to be plugged in. Embedded notebook cameras may also work, depending on the model.

Shortcuts
---------
* Space - take a picture.

Known issues
------------
<http:///crbug.com/?q=Cr%3DPlatform-Apps-Camera>
