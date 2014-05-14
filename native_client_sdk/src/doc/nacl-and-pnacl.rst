.. _nacl-and-pnacl:

##############
NaCl and PNaCl
##############

This document describes the differences between **Native Client** and
**Portable Native Client**, and provides recommendations for when to use each.

.. contents::
  :local:
  :backlinks: none
  :depth: 2

Native Client (NaCl)
====================

Native Client enables the execution of native code securely inside web
applications through the use of advanced `Software Fault Isolation (SFI)
techniques </native-client/community/talks#research>`_.  Since its launch in
2011, Native Client has provided developers with the ability to harness a
client machine's computational power to a much fuller extent than traditional
web technologies, by running compiled C and C++ code at near-native speeds and
taking advantage of multiple cores with shared memory.

While Native Client provides operating system independence, it requires
developers to generate architecture-specific executable modules
(**nexe** modules) for each hardware platform. This is not only inconvenient
for developers, but architecture-specific machine code is not portable and thus
not well-suited for the open web. The traditional method of application
distribution on the web is through a self-contained bundle of HTML, CSS,
JavaScript, and other resources (images, etc.) that can be hosted on a server
and run inside a web browser.  With this type of distribution, a website
created today should still work years later, on all platforms.
Architecture-specific executables are clearly not a good fit for distribution
on the web. As a consequence, Native Client has been restricted to
applications and browser extensions that are installed through the
Chrome Web Store.

Portable Native Client (PNaCl)
==============================

PNaCl solves the portability problem by splitting the compilation process
into two parts:

#. compiling the source code to a portable bitcode format, and
#. translating the bitcode to a host-specific executable.

PNaCl enables developers
to distribute **portable executables** (**pexe** modules) that the hosting
environment (e.g., the Chrome browser) can translate to native code before
executing. This portability aligns Native Client with existing open web
technologies such as JavaScript: A developer can distribute a **pexe**
as part of an application (along with HTML, CSS, and JavaScript),
and the user's machine is simply able to run it.

With PNaCl, a developer generates a single **pexe** from source code,
rather than multiple platform-specific nexes. The **pexe** provides both
architecture- and OS-independence. Since the **pexe** uses an abstract,
architecture-independent format, it does not suffer from the portability
problem described above. Future versions of hosting environments should
have no problem executing the **pexe**, even on new architectures.
Moreover, if an existing architecture is subsequently enhanced, the
**pexe** doesn't even have to be recompiled---in some cases the
client-side translation will automatically be able to take advantage of
the new capabilities.

**In short, PNaCl combines the portability of existing web technologies with
the performance and security benefits of Native Client.**

With the advent of PNaCl, the distribution restriction of Native Client
can be lifted. Specifically, a **pexe** module can be part of any web
application---it does not have to be distributed through the Chrome Web
Store.

PNaCl is a new technology, and as such it still has a few limitations
as compared to NaCl. These limitations are described below.

When to use PNaCl
=================

PNaCl is the preferred toolchain for Native Client, and the only way to deploy
Native Client modules on the open web. Unless your project is subject to one
of the narrow limitations described below
(see :ref:`When to use NaCl<when-to-use-nacl>`), you should use PNaCl.

Beginning with version 31, the Chrome browser supports translation of
**pexe** modules and their use in web applications, without requiring
any installation (either of a browser plugin or of the applications
themselves). Native Client and PNaCl are open-source technologies, and
our hope is that they will be added to other hosting platforms in the
future.

If controlled distribution through the Chrome Web Store is an important part
of your product plan, the benefits of PNaCl are less critical for you. But
you can still use the PNaCl toolchain and distribute your application
through the Chrome Web Store, and thereby take advantage of the
conveniences of PNaCl, such as not having to explicitly compile your application
for all supported architectures.

.. _when-to-use-nacl:

When to use NaCl
================

The limitations below apply to the current release of PNaCl. If any of
these limitations are critical for your application, you should use
non-portable NaCl:

* By its nature, PNaCl does not support architecture-specific
  instructions in an application (i.e., inline assembly), but tries to
  offer high-performance portable equivalents. One such example is
  PNaCl's :ref:`Portable SIMD Vectors <portable_simd_vectors>`.
* Currently PNaCl only supports static linking with the ``newlib``
  C standard library (the Native Client SDK provides a PNaCl port of
  ``newlib``). Dynamic linking and ``glibc`` are not yet supported.
  Work is under way to enable dynamic linking in future versions of PNaCl.
* In the initial release, PNaCl does not support some GNU extensions
  like taking the address of a label for computed ``goto``, or nested
  functions.
