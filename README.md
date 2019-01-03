# cts-experiment

```sh
cd cts-experiment/
npm install

npm install -g grunt-cli
grunt
```

After `build` and `serve`, open:
* http://localhost:8080/?suite=cts
* http://localhost:8080/?suite=unittests

## TODO

* `pfilter()` or `params.skip([{...}])`
* `--gtest_filter` equivalent
  * `npm run unittests` = `npm run cts -- --filter='unittests'`
