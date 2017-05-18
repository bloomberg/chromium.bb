# Prefetching Offline Pages: development guidelines

* Implementations that are injected dependencies should always provide
  lightweight construction and postpone heavier initialization (i.e. DB
  connection) to a later moment. Lazy initialization upon first actual usage is
  recommended.
