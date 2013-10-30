============================
PNaCl C/C++ Language Support
============================

.. contents::
   :local:
   :backlinks: none
   :depth: 3

Source language support
=======================

The currently supported languages are C and C++. The PNaCl toolchain is
based on Clang 3.3, which fully supports C++11 and most of C11. A
detailed status of the language support is available `here
<http://clang.llvm.org/cxx_status.html>`_.

For information on using languages other than C/C++, see the :ref:`FAQ
section on other languages <other_languages>`.

As for the standard libraries, the PNaCl toolchain is currently based on
``libstdc++`` version 4.6.1, and the ``newlib`` standard C library
(version is available through the macro ``NEWLIB_VERSION``).
Experimental ``libc++`` support is also included; see
:ref:`building_cpp_libraries` for more details.

Preprocessor definitions
------------------------

When compiling C/C++ code, the PNaCl toolchain defines the ``__pnacl__``
macro. In addition, ``__native_client__`` is defined for compatibility
with other NaCl toolchains.

.. _memory_model_and_atomics:

Memory Model and Atomics
========================

Memory Model for Concurrent Operations
--------------------------------------

The memory model offered by PNaCl relies on the same coding guidelines
as the C11/C++11 one: concurrent accesses must always occur through
atomic primitives (offered by `atomic intrinsics
<PNaClLangRef.html#atomicintrinsics>`_), and these accesses must always
occur with the same size for the same memory location. Visibility of
stores is provided on a happens-before basis that relates memory
locations to each other as the C11/C++11 standards do.

Non-atomic memory accesses may be reordered, separated, elided or fused
according to C and C++'s memory model before the pexe is created as well
as after its creation.

As in C11/C++11 some atomic accesses may be implemented with locks on
certain platforms. The ``ATOMIC_*_LOCK_FREE`` macros will always be
``1``, signifying that all types are sometimes lock-free. The
``is_lock_free`` methods and ``atomic_is_lock_free`` will return the
current platform's implementation at translation time. These macros,
methods and functions are in the C11 header ``<stdatomic.h>`` and the
C++11 header ``<atomic>``.

The PNaCl toolchain supports concurrent memory accesses through legacy
GCC-style ``__sync_*`` builtins, as well as through C11/C++11 atomic
primitives. ``volatile`` memory accesses can also be used, though these
are discouraged. See `Volatile Memory Accesses`_.

PNaCl supports concurrency and parallelism with some restrictions:

* Threading is explicitly supported and has no restrictions over what
  prevalent implementations offer. See `Threading`_.

* ``volatile`` and atomic operations are address-free (operations on the
  same memory location via two different addresses work atomically), as
  intended by the C11/C++11 standards. This is critical in supporting
  synchronous "external modifications" such as mapping underlying memory
  at multiple locations.

* Inter-process communication through shared memory is currently not
  supported. See `Future Directions`_.

* Signal handling isn't supported, PNaCl therefore promotes all
  primitives to cross-thread (instead of single-thread). This may change
  at a later date. Note that using atomic operations which aren't
  lock-free may lead to deadlocks when handling asynchronous
  signals. See `Future Directions`_.

* Direct interaction with device memory isn't supported, and there is no
  intent to support it. The embedding sandbox's runtime can offer APIs
  to indirectly access devices.

Setting up the above mechanisms requires assistance from the embedding
sandbox's runtime (e.g. NaCl's Pepper APIs), but using them once setup
can be done through regular C/C++ code.

Atomic Memory Ordering Constraints
----------------------------------

Atomics follow the same ordering constraints as in regular C11/C++11,
but all accesses are promoted to sequential consistency (the strongest
memory ordering) at pexe creation time. We plan to support more of the
C11/C++11 memory orderings in the future.

Some additional restrictions, following the C11/C++11 standards:

- Atomic accesses must at least be naturally aligned.
- Some accesses may not actually be atomic on certain platforms,
  requiring an implementation that uses global locks.
- An atomic memory location must always be accessed with atomic
  primitives, and these primitives must always be of the same bit size
  for that location.
- Not all memory orderings are valid for all atomic operations.

Volatile Memory Accesses
------------------------

The C11/C++11 standards mandate that ``volatile`` accesses execute in
program order (but are not fences, so other memory operations can
reorder around them), are not necessarily atomic, and can’t be
elided. They can be separated into smaller width accesses.

Before any optimizations occur, the PNaCl toolchain transforms
``volatile`` loads and stores into sequentially consistent ``volatile``
atomic loads and stores, and applies regular compiler optimizations
along the above guidelines. This orders ``volatiles`` according to the
atomic rules, and means that fences (including ``__sync_synchronize``)
act in a better-defined manner. Regular memory accesses still do not
have ordering guarantees with ``volatile`` and atomic accesses, though
the internal representation of ``__sync_synchronize`` attempts to
prevent reordering of memory accesses to objects which may escape.

Relaxed ordering could be used instead, but for the first release it is
more conservative to apply sequential consistency. Future releases may
change what happens at compile-time, but already-released pexes will
continue using sequential consistency.

The PNaCl toolchain also requires that ``volatile`` accesses be at least
naturally aligned, and tries to guarantee this alignment.

The above guarantees ease the support of legacy (i.e. non-C11/C++11)
code, and combined with builtin fences these programs can do meaningful
cross-thread communication without changing code. They also better
reflect the original code's intent and guarantee better portability.

.. _language_support_threading:

Threading
=========

Threading is explicitly supported through C11/C++11's threading
libraries as well as POSIX threads.

Communication between threads should use atomic primitives as described
in `Memory Model and Atomics`_.

``setjmp`` and ``longjmp``
==========================

PNaCl and NaCl support ``setjmp`` and ``longjmp`` without any
restrictions beyond C's.

Inline Assembly
===============

Inline assembly isn't supported by PNaCl because it isn't portable. The
one current exception is the common compiler barrier idiom
``asm("":::"memory")``, which gets transformed to a sequentially
consistent memory barrier (equivalent to ``__sync_synchronize()``). In
PNaCl this barrier is only guaranteed to order ``volatile`` and atomic
memory accesses, though in practice the implementation attempts to also
prevent reordering of memory accesses to objects which may escape.

NaCl supports a fairly wide subset of inline assembly through GCC's
inline assembly syntax, with the restriction that the sandboxing model
for the target architecture has to be respected.

Future Directions
=================

SIMD
----

PNaCl currently doesn't support SIMD. We plan to add SIMD support in the
very near future.

NaCl supports SIMD.

Inter-Process Communication
---------------------------

Inter-process communication through shared memory is currently not
supported by PNaCl/NaCl. When implemented, it may be limited to
operations which are lock-free on the current platform (``is_lock_free``
methods). It will rely on the address-free properly discussed in `Memory
Model for Concurrent Operations`_.

Signal Handling
---------------

Signal handling from user code currently isn't supported by PNaCl. When
supported, the impact of ``volatile`` and atomics for same-thread signal
handling will need to be carefully detailed.

NaCl supports signal handling.

Exception Handling
------------------

PNaCl currently doesn't support exception handling. It supports the
usual ``-fno-exceptions`` flag, and by default it transforms all
``throw`` statements into ``abort``. We plan to add exception-handling
support in the very near future, and zero-cost exception handling soon
thereafter.

NaCl supports exception handling.

Computed ``goto``
-----------------

PNaCl currently doesn't support computed ``goto``, a non-standard
extension to C used by some interpreters.

NaCl supports computed ``goto``.
