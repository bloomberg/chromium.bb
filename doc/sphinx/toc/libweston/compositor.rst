Compositor
==========

:type:`weston_compositor` represents the core object of the library, which
aggregates all the other objects and maintains their state. You can create it
using :func:`weston_compositor_create`, while for releasing all the resources
associated with it, you should use :func:`weston_compositor_tear_down`,
followed by :func:`weston_compositor_destroy` to destroy it.

Compositor API
--------------

.. doxygengroup:: compositor
   :content-only:
