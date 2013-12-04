Mojo Public API
===============

The Mojo Public API is a binary stable API to the Mojo system. There are
several components to the API:

Bindings
--------

This directory contains a static library that clients can link into their
binary. The contents of this directory are not binary stable because each
client is free to use whichever version they prefer.

This directory also contains a compiler that translates mojom interface
definition files into idiomatic bindings for various languages, including
C++ and JavaScript. Clients are expected to statically link with the generated
code, which reads and writes the binary stable IPC message format.

GLES2
-----

The IPC protocol used to communicate between Mojo client and the GLES2
service is not binary stable. To insulate themselves from changes in this
protocol, clients are expected to link dynamically against the standard GLES2
headers from Khronos and the headers in this directory, which provide an
adaptor between the GLES2 C API and the underlying IPC protocol.

System
------

This directory defines the interface between Mojo clients and the Mojo IPC
system. Although the Mojo IPC message format is binary stable, the mechanism
by which these messages are transported is not stable. To insulate themselves
from changes in the underlying transport, clients are expected to link against
these headers dynamically.

Tests
-----

This directory contains tests for code contained in the public API. Mojo
clients are expected to ignore this directory.
