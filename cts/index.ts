import Jasmine from 'jasmine';
const jasmine = new Jasmine({});

Promise.all([
  import('./hello'),
]).then(async trees => {
  trees.map(tree => tree.default());
}).then(() => {
  jasmine.execute();
});
