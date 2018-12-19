# cts-experiment

```sh
cd cts-experiment/
npm install

npm run cts # run cts (includes framework unittest)

npm run build # build for web (without checking types) (run via index.html)
npm run clean # delete build

npm run check # just check types
npm run lint # tslint
npm run fix # tslint --fix
```

## TODO

* `pfilter()` or `params.skip([{...}])`
* `--gtest_filter` equivalent
  * `npm run unittests` = `npm run cts -- --filter='unittests'`
