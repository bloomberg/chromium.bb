## Introduction

The `.gyp` files in the NaCl source tree serve two purposes: they are used to
build NaCl as part of Chromium (see ChromiumIntegration), and they can also be
used to build NaCl standalone, as an alternative to the Scons build scripts.

## Standalone Linux Gyp build

In the top-level (`trunk/src`) directory: * `./native_client/build/gyp_nacl
native_client/build/all.gyp -f make` * `make -j4`

## Standalone Windows Gyp build

In the top-level (`trunk/src`) directory: * If necessary: run `vcvarsall.bat`
from the Visual Studio/VC directory. * `set GYP_MSVS_VERSION=2008` * `gclient
runhooks` * `devenv native_client\build\all.sln /build Debug` It also works from
the `native_client` directory: * `devenv build\all.sln /build Debug`

## Tests

Most of the NaCl tests cannot be run using the Gyp build, because the Gyp build
does not know how to invoke the NaCl compiler. Gyp does not handle
cross-compilation well in general. To run the NaCl tests you have to use the
Scons build.
