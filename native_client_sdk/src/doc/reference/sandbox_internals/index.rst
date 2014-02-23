.. _sandbox-internals-index:

#################
Sandbox Internals
#################

The sandbox internals documentation describes implementation details for
Native Client sandboxing, which is also used by Portable Native
Client. These details can be useful to reimplement a sandbox, or to
write assembly code that follows sandboxing rules for Native Client
(Portable Native Client does not allow platform-specific assembly code).

Native Client has sandboxes for:

* :ref:`ARM 32-bit<arm-32-bit-sandbox>`.
* x86-32
* x86-64
* MIPS32
