# Startup Tracing with MemoryInfra

[MemoryInfra](memory_infra.md) supports startup tracing.

## The Simple Way

Start Chrome as follows:

    $ chrome --no-sandbox \
             --trace-startup=-*,disabled-by-default-memory-infra \
             --trace-startup-file=/tmp/trace.json \
             --trace-startup-duration=7

This will use the default configuration: one memory dump every 250 ms with a
detailed dump ever two seconds.

## The Advanced Way

If you need more control over the granularity of the memory dumps, you can
specify a custom trace config file as follows:

    $ cat > /tmp/trace.config
    {
      "startup_duration": 4,
      "result_file": "/tmp/trace.json",
      "trace_config": {
        "included_categories": ["disabled-by-default-memory-infra"],
        "excluded_categories": ["*"],
        "memory_dump_config": {
          "triggers": [
            { "mode": "light", "periodic_interval_ms": 50 },
            { "mode": "detailed", "periodic_interval_ms": 1000 }
          ]
        }
      }
    }

    $ chrome --no-sandbox --trace-config-file=/tmp/trace.config

## Related Pages

 * [General information about startup tracing](https://sites.google.com/a/chromium.org/dev/developers/how-tos/trace-event-profiling-tool/recording-tracing-runs)
 * [Memory tracing with MemoryInfra](memory_infra.md)
