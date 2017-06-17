# Prefetching Offline Pages

## Architecture overview

### PrefetchService

Is the ownership holder for the main components of the prefetching system and
controls their lifetime.

### PrefetchDispatcher

Manages the prefetching pipeline tasks. It receives signals from external
clients and creates the appropriate tasks to execute them. It _might_ at some
point execute advanced task management operations like canceling queued tasks or
changing their order of execution.

### \*Task(s) (i.e. AddUniqueUrlsTask)

They are the main wrapper of pipeline steps and interact with different 
abstracted components (Downloads, persistent store, GCM, etc) to execute them.
They implement TaskQueue's Task API so that they can be exclusively executed.

## Development guidelines

* Implementations that are injected dependencies during service creation should
  have lightweight construction and postpone heavier initialization (i.e. DB
  connection) to a later moment. Lazy initialization upon first actual usage is
  recommended.
