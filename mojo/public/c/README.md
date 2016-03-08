Mojo Public C API
=================

This directory contains C language bindings for the Mojo Public API.

GLES2
-----

TODO(yzshen): move GLES2 to where it belongs (likely components/mus).

The gles2/ subdirectory defines the GLES2 C API that's available to Mojo
applications. To use GLES2, Mojo applications must link against a dynamic
library (the exact mechanism being platform-dependent) and use the header files
in this directory as well as the standard Khronos GLES2 header files.

The reason for this, rather than providing GLES2 using the standard Mojo IPC
mechanism, is performance: The protocol (and transport mechanisms) used to
communicate with the Mojo GLES2 service is not stable nor "public" (mainly for
performance reasons), and using the dynamic library shields the application from
changes to the underlying system.

System
------

The system/ subdirectory provides definitions of the basic low-level API used by
all Mojo applications (whether directly or indirectly). These consist primarily
of the IPC primitives used to communicate with Mojo services.

Though the message protocol is stable, the implementation of the transport is
not, and access to the IPC mechanisms must be via the primitives defined in this
directory.

Test Support
------------

This directory contains a C API for running tests. This API is only available
under special, specific test conditions. It is not meant for general use by Mojo
applications.
